
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/Compiler.h"
#include "LAMPProfiling.h"

#include "llvm/Analysis/LoopPass.h"	//TRM 7/21/08
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"	//TRM 7/21/08

#include "llvm/Support/Debug.h"
#include <iostream>
#include <set>
#include <fstream>
#include <string>

using namespace llvm;
using namespace std;

// This class is a module pass designed to do no modification or instrumentation but count the number of
// loads, stores, and calls for the initialization call.  It also tracks the loop counts generated by the
// loop profiler so they can be accessed by the initializing pass.
namespace {
	class LdStCallCounter : public ModulePass {
		public:
			static char ID;
			static bool flag;
			bool runOnModule(Module &M);
			static unsigned int num_loads;
			static unsigned int num_stores;
			static unsigned int num_calls;
			static unsigned int num_loops;
			LdStCallCounter(): ModulePass(ID)
		{

		}	
			unsigned int getCountInsts() { return num_loads + num_stores + num_calls; }
	};

}

char LdStCallCounter::ID = 0;

// flag to ensure we only count once
bool LdStCallCounter::flag = false;

// only want these counted once and only the first time (not after other instrumentation)
unsigned int LdStCallCounter::num_loads = 0;	
unsigned int LdStCallCounter::num_stores = 0;
unsigned int LdStCallCounter::num_calls = 0;
// store loops here also because loop passes cannot be required by other passes
unsigned int LdStCallCounter::num_loops = 0;	

static RegisterPass<LdStCallCounter>
Y("lamp-insts",
		"Count the number of LAMP Profilable insts");

ModulePass *llvm::createLdStCallCounter() {
	return new LdStCallCounter();
}

bool LdStCallCounter::runOnModule(Module &M) {

	if (flag == true)	// if we have counted already -- structure of llvm means this could be called many times
		return false;
	// for all functions in module
	for (Module::iterator I = M.begin(), E = M.end(); I != E; ++I)
		if (!I->isDeclaration())
		{			// for all blocks in the function
			for (Function::iterator BBB = I->begin(), BBE = I->end(); BBB != BBE; ++BBB)
			{		// for all instructions in a block
				for (BasicBlock::iterator IB = BBB->begin(), IE = BBB->end(); IB != IE; IB++)
				{
					if (isa<LoadInst>(IB))		// count loads, stores, calls
						num_loads++;
					else if (isa<StoreInst>(IB))
						num_stores++;		// count only external calls, ignore declarations, etc
					else if (isa<CallInst>(IB) && ( (dyn_cast<CallInst>(IB)->getCalledFunction() == NULL) || 
								(dyn_cast<CallInst>(IB)->getCalledFunction()->isDeclaration())))
						num_calls++;
				}
			}
		}
	//DOUT << "Loads/Store/Calls:" << num_loads << " " << num_stores << " " << num_calls << std::endl;
	flag = true;

	return false;
}

// LAMPProfiler instruments loads, stores, and calls.  Target data required to determine
// data size to be profiled.
namespace {
	class LAMPProfiler : public FunctionPass {
		bool runOnFunction(Function& F);

		Constant* lampFuncs[9];
		Constant* CallFn;
		Constant* AllocFn;
		Constant* DeallocFn;

		void createLampDeclarations(Module* M);
		int getIndex(const Type* ty);
		TargetData* TD;
		public:
		virtual void getAnalysisUsage(AnalysisUsage &AU) const {
			AU.addRequired<TargetData>();
		}

		bool doInitialization(Module &M) { return false; }
		static unsigned int instruction_id;
		static char ID;
		LAMPProfiler() : FunctionPass(ID) 
		{ //instruction_id = 0; 
			lampFuncs[0] = NULL;
			TD = NULL; } 

		long getLLFIIndexofInst(Instruction *I);
		void dumpIdMap(unsigned int id, long index);
		void dumpOpcodeMap(long index, int opcode);
	};
}

char LAMPProfiler::ID = 0;
unsigned int LAMPProfiler::instruction_id = -1;

static RegisterPass<LAMPProfiler>
X("insert-lamp-profiling",
		"Insert instrumentation for LAMP profiling");

FunctionPass *llvm::createLAMPProfilerPass() { return new LAMPProfiler(); }

void LAMPProfiler::createLampDeclarations(Module* M)
{
	std::string f[] = {"LAMP_load1", "LAMP_load2", "LAMP_load4", "LAMP_load8", 
		"LAMP_store1", "LAMP_store2",	"LAMP_store4", "LAMP_store8"};

	std::string FnName = "LAMP_register";
	std::string AllocName = "LAMP_allocate";
	std::string DeallocName = "LAMP_deallocate";

	for (int i=0; i < 4; i++)
	{
		lampFuncs[i] = M->getOrInsertFunction(f[i], llvm::Type::getVoidTy(M->getContext()), llvm::Type::getInt32Ty(M->getContext()),
				llvm::Type::getInt64Ty(M->getContext()), (Type *)0);
		lampFuncs[i+4] = M->getOrInsertFunction(f[i+4], llvm::Type::getVoidTy(M->getContext()), llvm::Type::getInt32Ty(M->getContext()), llvm::Type::getInt64Ty(M->getContext()), llvm::Type::getInt64Ty(M->getContext()), (Type *)0);
	} 

	CallFn = M->getOrInsertFunction(FnName, llvm::Type::getVoidTy(M->getContext()), llvm::Type::getInt32Ty(M->getContext()), (Type *)0);

	// args are unsigned32, void*, size_t ... seems to translate as these  TRM 7/25/08
	// should be 32, 8?, 32 -- currently defined as 32 64 64
	AllocFn = M->getOrInsertFunction(AllocName, llvm::Type::getVoidTy(M->getContext()), llvm::Type::getInt32Ty(M->getContext()), llvm::Type::getInt8Ty(M->getContext()), llvm::Type::getInt32Ty(M->getContext()), (Type*)0);
	DeallocFn = M->getOrInsertFunction(DeallocName, llvm::Type::getVoidTy(M->getContext()), llvm::Type::getInt32Ty(M->getContext()), llvm::Type::getInt8Ty(M->getContext()), llvm::Type::getInt32Ty(M->getContext()), (Type*)0);
}

int LAMPProfiler::getIndex(const Type* ty)
{
	int i = TD->getTypeSizeInBits(ty);
	switch (i)
	{
		case 8:
			return 0;	// load size 1 byte, function #0 or 4
		case 16:
			return 1;	// load size 2 bytes, function #1 or 5
		case 32:
			return 2;	// load size 4 bytes, function #2 or 6
		case 64:
			return 3;	// load size 8 bytes , function #3 or 7
		default:
			return 0;
	}
}

bool LAMPProfiler::runOnFunction(Function &F) {

	if (lampFuncs[0] == NULL)
	{
		Module* M = F.getParent();
		createLampDeclarations(M);
	}

	//DOUT << "Instrumenting Function " << F.getName() << " beginning ID: " << instruction_id << std::endl;

	if (TD == NULL)
		TD = &getAnalysis<TargetData>();
	for (Function::iterator IF = F.begin(), IE = F.end(); IF != IE; ++IF)
	{

		BasicBlock& BB = *IF;

		for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E; ++I)
		{

			// Dump map between lampview id and llfi index
			long llfiIndex = LAMPProfiler::getLLFIIndexofInst(I);

			// Dump map between llfi index and instruction opcode
			//int opcode = I->getOpcode();
			//dumpOpcodeMap(llfiIndex, opcode);

/*
			// Add doProfile function call to every llfi injectable instruction
			if (llfiIndex >= 0){
				// Add doProfiling(opcode)
				Function *dphook;
				Constant *dphookFunc;
				dphookFunc = F.getParent()->getOrInsertFunction("doProfiling", Type::getVoidTy(F.getParent()->getContext()), Type::getInt64Ty(F.getParent()->getContext()), (Type *)0);
				dphook= cast<Function>(dphookFunc);
				//if(I->getOpcode() != 28 && I->getOpcode() != 2 && I->getOpcode() != 1){ // TODO Handle this in doProfiling function
				//Value* opcodeValue = ConstantInt::get(Type::getInt32Ty(F.getParent()->getContext()), I->getOpcode());
				Value* llfiIndexValue = ConstantInt::get(Type::getInt64Ty(F.getParent()->getContext()), llfiIndex);
				CallInst* call_dp = CallInst::Create(dphook, llfiIndexValue, "");
				IF->getInstList().insert(I, call_dp);
				//}

				   Function *maphook;
				   Constant *maphookFunc;
				   maphookFunc = F.getParent()->getOrInsertFunction("doMap", Type::getVoidTy(F.getParent()->getContext()), Type::getInt64Ty(F.getParent()->getContext()), (Type *)0);
				   maphook= cast<Function>(maphookFunc);
				   Value* llfiIndexValue = ConstantInt::get(Type::getInt64Ty(F.getParent()->getContext()), llfiIndex);
				//if(I->getOpcode() != 28 && I->getOpcode() != 2 && I->getOpcode() != 1){ // Handle this in doProfiling function
				CallInst* call_map = CallInst::Create(dphook, llfiIndexValue, "");
				IF->getInstList().insert(I, call_map);

			}
				 */

			// Instrument Loads
			if (isa<LoadInst>(I) && llfiIndex >=0 )
			{

				std::vector<Value*> Args(2);

				Args[0] = ConstantInt::get(llvm::Type::getInt32Ty(F.getContext()), ++instruction_id);

				Value* ptr= (dyn_cast<LoadInst>(I))->getPointerOperand();
				Args[1] = new PtrToIntInst(ptr, llvm::Type::getInt64Ty(F.getContext()), "addr_var", I);

				Value* v = new LoadInst(ptr, "value_var", I);

				//				int index = getIndex(Args[1]->getType());  

				int index = getIndex(v->getType());        
				// cerr << index << " " << *I  ; // DEBUG

				CallInst::Create(lampFuncs[index], Args.begin(), Args.end(), "", I);

				// errs() << llfiIndex << '\n';
				LAMPProfiler::dumpIdMap(instruction_id, llfiIndex);
			}
			// Instrument Stores
			else if (isa<StoreInst>(I) && llfiIndex >=0)
			{

				std::vector<Value*> Args(3);

				Args[0] = ConstantInt::get(llvm::Type::getInt32Ty(F.getContext()), ++instruction_id);

				Value* ptr= (dyn_cast<StoreInst>(I))->getPointerOperand();
				Args[1] = new PtrToIntInst(ptr, llvm::Type::getInt64Ty(F.getContext()), "addr_var", I);

				Value* v = (dyn_cast<StoreInst>(I))->getOperand(0);
				const Type*  Op_0_Type = v->getType();

				// fp ( cast to int64 )
				if(Op_0_Type->getTypeID() == llvm::Type::FloatTyID || Op_0_Type->getTypeID() == llvm::Type::DoubleTyID){
					Args[2]= new FPToSIInst(v, llvm::Type::getInt64Ty(F.getContext()), "value_var", I);
					// ptr (cast to int64)
				}else if(Op_0_Type->getTypeID() == llvm::Type::PointerTyID){
					Args[2]= new PtrToIntInst(v, llvm::Type::getInt64Ty(F.getContext()), "value_var", I);
					// int64 
				}else if (TD->getTypeSizeInBits(Op_0_Type) == 64)
					Args[2] = v;
				// int (sign extended to int64)
				else { 
					// cerr << TD->getTypeSizeInBits(I->getOperand(0)->getType()) << " " << *(I->getOperand(0)->getType()) << std::endl; // DEBUG
					if(TD->getTypeSizeInBits(Op_0_Type) < 64)
						Args[2] = new SExtInst(v, llvm::Type::getInt64Ty(F.getContext()), "value_var", I);
					else
					{
						dbgs()<<"\nFound type bits > 64! :" << *I <<"\n";
						Args[2] = new TruncInst(v, llvm::Type::getInt64Ty(F.getContext()), "value_var", I);
					}
				}

				//				int index = getIndex(Args[1]->getType()) + 4;     // CHECK
				// pochun : this might not be right. the type of varible should be used, 
				// not the type of of pointer (casted to i64). The next line is the fix.
				int index = getIndex(Op_0_Type) + 4;
				// cerr << index << " " << *I  << std::endl;  // DEBUG
				CallInst::Create(lampFuncs[index], Args.begin(), Args.end(), "", I);		      

				LAMPProfiler::dumpIdMap(instruction_id, llfiIndex);

			} 
			// Instrument external calls
			else if (isa<CallInst>(I) && llfiIndex >=0 && ( (dyn_cast<CallInst>(I)->getCalledFunction() == NULL) || 
						(dyn_cast<CallInst>(I)->getCalledFunction()->isDeclaration())))
			{
				std::vector<Value*> Args(1);

				Args[0] = ConstantInt::get(llvm::Type::getInt32Ty(F.getContext()), ++instruction_id);

				CallInst::Create(CallFn, Args.begin(), Args.end(), "", I); 
			}
			/*			else if (isa<AllocationInst>(I))
						{
						std::vector<Value*> ListOfInst(10);
						int i = 0;

						while (isa<AllocaInst>(I))
						{
						ListOfInst[i] = I;
						i++;
						I++;				
						}

						int i2;

			//cerr << *I << std::endl;
			for (i2 = 0; i2 < i; i2++)
			{			
			std::vector<Value*> Args(3);

			Args[0] = ConstantInt::get(llvm::Type::getInt32Ty(F.getContext()), 0);

			Value* tempaddr = new PtrToIntInst(ListOfInst[i2], llvm::Type::getInt64Ty(F.getContext()), "addr_var", I);
			Args[1] = new TruncInst(tempaddr, llvm::Type::Int8Ty, "addr_var", I);

			Value* size = (dyn_cast<AllocationInst>)(ListOfInst[i2])->getArraySize();
			Value* tempval = new SExtInst(size, llvm::Type::getInt64Ty(F.getContext()), "temp_var", I); // should be 32

			Args[2] = new TruncInst(tempval, llvm::Type::getInt32Ty(F.getContext()), "size_var", I);

			CallInst::Create(AllocFn, Args.begin(), Args.end(), "", I);
			}
			// possibly insert LAMP_allocate call here -- u32 lampID, void * memory, size_t size
			}
			else if (isa<FreeInst>(I))
			{

			std::vector<Value*> Args(3);				

			Args[0] = ConstantInt::get(llvm::Type::getInt32Ty(F.getContext()), 0);

			//Value* ptr= (III.getOperand(0));
			//const Type *SrcTy = ptr->getType();
			//DOUT << "Source: " << *SrcTy;
			//Args[1] = new SExtInst(ptr, llvm::Type::getInt32Ty(F.getContext()), "addr_var", I);

			Value* tempaddr = new PtrToIntInst(I, llvm::Type::getInt64Ty(F.getContext()), "taddr_var", I);
			Args[1] = new TruncInst(tempaddr, llvm::Type::Int8Ty, "addr_var", I);

			Value* size = (dyn_cast<AllocationInst>)(I)->getArraySize();
			Value* tempval = new SExtInst(size, llvm::Type::getInt64Ty(F.getContext()), "temp_var", I);  // should be 32

			Args[2] = new TruncInst(tempval, llvm::Type::getInt32Ty(F.getContext()), "size_var", I);

			CallInst::Create(DeallocFn, Args.begin(), Args.end(), "", I);
			DOUT << instruction_id << *I;

			// possibly insert LAMP_deallocate call here -- u32 lampID, void * memory, size_t size
			}*/
			}
		}

		//DOUT << "Instrumentation of " << F.getName() << " complete.  Ending ID: " << instruction_id << std::endl;

		return true;
	}




	// This class retrieves data from the LdStCallCounter class.  While not explicitly noted for llvm structural
	// reasons, this class does require that insert-lamp-loop-profiling (LAMPLoopProfiler class) run first.  If it
	// fails to run first, the number of loops will be reported as zero.  Initialization pass should be run LAST.
	namespace {
		class LAMPInit : public ModulePass {
			bool runOnModule(Module& M);

			public:
			virtual void getAnalysisUsage(AnalysisUsage &AU) const {
				AU.addRequired<LdStCallCounter>();
				//  AU.addRequired<LAMPLoopProfiler>();  LAMPLoopProfiler MUST run first but we cannot add required due to 
			}						// LLVM structural issues

			static char ID;
			LAMPInit() : ModulePass(ID) 
			{ } 
		};
	}

	char LAMPInit::ID = 0;

	static RegisterPass<LAMPInit>
		V("insert-lamp-init",
				"Insert initialization for LAMP profiling");

	ModulePass *llvm::createLAMPInitPass() { return new LAMPInit(); }

	bool LAMPInit::runOnModule(Module& M)
	{
		for(Module::iterator IF = M.begin(), E = M.end(); IF != E; ++IF)
		{
			Function& F = *IF;
			if (F.getName() == "main") {
				const char* FnName = "LAMP_init";

				LdStCallCounter& lscnts = getAnalysis<LdStCallCounter>();

				unsigned int cnt = lscnts.getCountInsts();
				unsigned int lps = lscnts.num_loops;

				//DOUT << "LAMP-- Ld/St/Call Count:" << cnt << " Loop Count:" << lps <<std::endl;

				Constant *InitFn = M.getOrInsertFunction(FnName, llvm::Type::getVoidTy(M.getContext()), llvm::Type::getInt32Ty(M.getContext()), llvm::Type::getInt32Ty(M.getContext()), llvm::Type::getInt64Ty(M.getContext()), llvm::Type::getInt64Ty(M.getContext()),(Type *)0);
				BasicBlock& entry = F.getEntryBlock();
				BasicBlock::iterator InsertPos = entry.begin();
				while (isa<AllocaInst>(InsertPos)) ++InsertPos;

				std::vector<Value*> Args(4);
				Args[0] = ConstantInt::get(llvm::Type::getInt32Ty(M.getContext()), cnt, false);
				Args[1] = ConstantInt::get(llvm::Type::getInt32Ty(M.getContext()), lps, false);
				Args[2] = ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), 1, false);
				Args[3] = ConstantInt::get(llvm::Type::getInt64Ty(M.getContext()), 0, false);

				CallInst::Create(InitFn, Args.begin(), Args.end(), "", InsertPos);														
				return true;
			}
		}
		return false;
	}

	// Loop instrumentation class instruments loops with invocation, iteration beginning, iteration ending
	// and loop exiting calls.  It also counts the number of loops for use by LAMPProfiler initilization.
	namespace {
		class LAMPLoopProfiler : public LoopPass {
			bool runOnLoop (Loop *Lp, LPPassManager &LPM);

			virtual void getAnalysisUsage(AnalysisUsage &AU) const {
				AU.addRequiredTransitive<LdStCallCounter>();
				//AU.addRequired<LAMPProfiler>();	For reasons incomprehensible to us, this is not permissible
			}

			unsigned int numLoops;			// numLoops for LAMPProfiler initilization

			public:
			bool doInitialization(Loop *Lp, LPPassManager &LPM) { return false; }
			static char ID;
			static bool IDInitFlag;
			static unsigned int loop_id;		// ids will be progressive starting after instruction ids
			LAMPLoopProfiler() : LoopPass(ID) 
			{  
				numLoops = 0;
			}	 
			unsigned int getNumLoops(){ return numLoops;}
		};
	}

	char LAMPLoopProfiler::ID = 0;
	bool LAMPLoopProfiler::IDInitFlag = false;
	unsigned int LAMPLoopProfiler::loop_id = 0;

	static RegisterPass<LAMPLoopProfiler>
		W("insert-lamp-loop-profiling",
				"Insert instrumentation for LAMP loop profiling");

	LoopPass *llvm::createLAMPLoopProfilerPass() { return new LAMPLoopProfiler(); }


	bool LAMPLoopProfiler::runOnLoop(Loop *Lp, LPPassManager &LPM) {
		BasicBlock *preHeader;
		BasicBlock *header;
		BasicBlock *latch;

		LdStCallCounter& lscnts = getAnalysis<LdStCallCounter>();

		if(!IDInitFlag)
		{
			loop_id = lscnts.getCountInsts()-1;	// first id will begin after instruction ids
			IDInitFlag = true;
		}

		SmallVector<BasicBlock*, 8> exitBlocks;			// assuming max 8 exit blocks.  Is this wise?
		// TRM 7/24/08 removed exiting blocks instrumentation
		// in favor of placing iter end prior loop exit
		header = Lp->getHeader();
		preHeader = Lp->getLoopPreheader();
		latch = Lp->getLoopLatch();

		Lp->getExitBlocks(exitBlocks);

		Module *M = (header->getParent())->getParent();

		numLoops++;

		lscnts.num_loops = numLoops;

		// insert invocation function at end of preheader (called once prior to loop)
		const char* InvocName = "LAMP_loop_invocation";
		Constant *InvocFn = M->getOrInsertFunction(InvocName, llvm::Type::getVoidTy(M->getContext()), llvm::Type::getInt32Ty(M->getContext()), (Type *)0);
		std::vector<Value*> Args(1);
		Args[0] = ConstantInt::get(llvm::Type::getInt32Ty(M->getContext()), ++loop_id);

		// Justin: only instrument in indexed functions
		Function* F = preHeader->getParent();
		std::string functionName = F->getName();
		if( functionName != "getOpcode" ){
			if (!preHeader->empty())

				CallInst::Create(InvocFn, Args.begin(), Args.end(), "", (preHeader->getTerminator()));
			else
				CallInst::Create(InvocFn, Args.begin(), Args.end(), "", (preHeader));


			// insert iteration begin function at beginning of header (called each loop)
			const char* IterBeginName = "LAMP_loop_iteration_begin";
			Constant *IterBeginFn = M->getOrInsertFunction(IterBeginName, llvm::Type::getVoidTy(M->getContext()), (Type *)0);	

			// find insertion point (after PHI nodes) -KF 11/18/2008
			for (BasicBlock::iterator ii = header->begin(), ie = header->end(); ii != ie; ++ii) {
				if (!isa<PHINode>(ii)) {
					CallInst::Create(IterBeginFn, "", ii);
					break;
				}
			}

			// insert iteration at cannonical backedge.  exiting block insertions removed in favor of exit block
			const char* IterEndName = "LAMP_loop_iteration_end";
			Constant *IterEndFn = M->getOrInsertFunction(IterEndName, llvm::Type::getVoidTy(M->getContext()), (Type *)0);	

			// cannonical backedge
			if (!latch->empty())
				CallInst::Create(IterEndFn, "", (latch->getTerminator()));
			else
				CallInst::Create(IterEndFn, "", (latch));


			// insert loop end at beginning of exit blocks
			const char* LoopEndName = "LAMP_loop_exit";
			Constant *LoopEndFn = M->getOrInsertFunction(LoopEndName, llvm::Type::getVoidTy(M->getContext()), (Type *)0);	

			set <BasicBlock*> BBSet; 
			BBSet.clear();
			for(unsigned int i = 0; i != exitBlocks.size(); i++){		
				// this ordering places iteration end before loop exit
				// make sure not inserting the same exit block more than once for a loop -PC 2/5/2009
				if (BBSet.find(exitBlocks[i])!=BBSet.end()) continue;
				BBSet.insert(exitBlocks[i]);
				// find insertion point (after PHI nodes) -PC 2/2/2009  -TODO: there is some function to do this.
				BasicBlock::iterator ii =  exitBlocks[i]->begin();
				while (isa<PHINode>(ii)) { ii++; }
				CallInst::Create(IterEndFn, "", ii);	// iter end placed before exit call
				CallInst::Create(LoopEndFn, "", ii);	// loop exiting
			}
		}
		//DOUT << "Num Loops Processed: " << numLoops << "  Loop ID: " << loop_id << std::endl;
		return true;	
	}

	long LAMPProfiler::getLLFIIndexofInst(Instruction *inst) {
		MDNode *mdnode = inst->getMetadata("llfi_index");
		if (mdnode) {
			ConstantInt *cns_index = dyn_cast<ConstantInt>(mdnode->getOperand(0));
			return cns_index->getSExtValue();
		}
		return -1;
	}

	void LAMPProfiler::dumpIdMap(unsigned int id, long index){
		FILE *f = fopen("result.lamp.id_index", "a");
		fprintf(f, "%d->%ld\n", id, index); // LAMPVIEW ID -> LLFI INDEX
		fclose(f);
	}


	void LAMPProfiler::dumpOpcodeMap(long index, int opcode){
		FILE *f = fopen("result.lamp.index_opcode", "a");
		fprintf(f, "%ld->%d\n", index, opcode); // LLFI INDEX->OPCODE
		fclose(f);
	}

