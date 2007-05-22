/*
 *  cHardwareGX.cc
 *  Avida
 *
 *  Copyright 1999-2007 Michigan State University. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#include <climits>
#include <fstream>
#include <algorithm>
#include "cHardwareGX.h"
#include "cAvidaContext.h"
#include "cCPUTestInfo.h"
#include "functions.h"
#include "cGenomeUtil.h"
#include "cGenotype.h"
#include "cHardwareManager.h"
#include "cHardwareTracer.h"
#include "cInstSet.h"
#include "cMutation.h"
#include "cMutationLib.h"
#include "nMutation.h"
#include "cOrganism.h"
#include "cPhenotype.h"
#include "cStringUtil.h"
#include "cTestCPU.h"
#include "cWorldDriver.h"
#include "cWorld.h"
#include "tInstLibEntry.h"


using namespace std;

//! A small helper struct to make deleting a little easier.
struct delete_functor {
  template <typename T> void operator()(T *ptr) { delete ptr; }
};


tInstLib<cHardwareGX::tMethod>* cHardwareGX::s_inst_slib = cHardwareGX::initInstLib();

tInstLib<cHardwareGX::tMethod>* cHardwareGX::initInstLib(void)
{
  struct cNOPEntryCPU {
    cString name;
    int nop_mod;
    cNOPEntryCPU(const cString &name, int nop_mod)
      : name(name), nop_mod(nop_mod) {}
  };
  static const cNOPEntryCPU s_n_array[] = {
    cNOPEntryCPU("nop-A", REG_AX),
    cNOPEntryCPU("nop-B", REG_BX),
    cNOPEntryCPU("nop-C", REG_CX)
  };
  
  static const tInstLibEntry<tMethod> s_f_array[] = {
    /*
     Note: all entries of cNOPEntryCPU s_n_array must have corresponding
     in the same order in tInstLibEntry<tMethod> s_f_array, and these entries must
     be the first elements of s_f_array.
     */
    tInstLibEntry<tMethod>("nop-A", &cHardwareGX::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation instruction; modifies other instructions"),
    tInstLibEntry<tMethod>("nop-B", &cHardwareGX::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation instruction; modifies other instructions"),
    tInstLibEntry<tMethod>("nop-C", &cHardwareGX::Inst_Nop, (nInstFlag::DEFAULT | nInstFlag::NOP), "No-operation instruction; modifies other instructions"),
    
    tInstLibEntry<tMethod>("NULL", &cHardwareGX::Inst_Nop, 0, "True no-operation instruction: does nothing"),
    tInstLibEntry<tMethod>("nop-X", &cHardwareGX::Inst_Nop, 0, "True no-operation instruction: does nothing"),
    tInstLibEntry<tMethod>("if-equ-0", &cHardwareGX::Inst_If0, 0, "Execute next instruction if ?BX?==0, else skip it"),
    tInstLibEntry<tMethod>("if-not-0", &cHardwareGX::Inst_IfNot0, 0, "Execute next instruction if ?BX?!=0, else skip it"),
    tInstLibEntry<tMethod>("if-n-equ", &cHardwareGX::Inst_IfNEqu, nInstFlag::DEFAULT, "Execute next instruction if ?BX?!=?CX?, else skip it"),
    tInstLibEntry<tMethod>("if-equ", &cHardwareGX::Inst_IfEqu, 0, "Execute next instruction if ?BX?==?CX?, else skip it"),
    tInstLibEntry<tMethod>("if-grt-0", &cHardwareGX::Inst_IfGr0),
    tInstLibEntry<tMethod>("if-grt", &cHardwareGX::Inst_IfGr),
    tInstLibEntry<tMethod>("if->=-0", &cHardwareGX::Inst_IfGrEqu0),
    tInstLibEntry<tMethod>("if->=", &cHardwareGX::Inst_IfGrEqu),
    tInstLibEntry<tMethod>("if-les-0", &cHardwareGX::Inst_IfLess0),
    tInstLibEntry<tMethod>("if-less", &cHardwareGX::Inst_IfLess, nInstFlag::DEFAULT, "Execute next instruction if ?BX? < ?CX?, else skip it"),
    tInstLibEntry<tMethod>("if-<=-0", &cHardwareGX::Inst_IfLsEqu0),
    tInstLibEntry<tMethod>("if-<=", &cHardwareGX::Inst_IfLsEqu),
    tInstLibEntry<tMethod>("if-A!=B", &cHardwareGX::Inst_IfANotEqB),
    tInstLibEntry<tMethod>("if-B!=C", &cHardwareGX::Inst_IfBNotEqC),
    tInstLibEntry<tMethod>("if-A!=C", &cHardwareGX::Inst_IfANotEqC),
    tInstLibEntry<tMethod>("if-bit-1", &cHardwareGX::Inst_IfBit1),
    
    tInstLibEntry<tMethod>("call", &cHardwareGX::Inst_Call),
    tInstLibEntry<tMethod>("return", &cHardwareGX::Inst_Return),

    tInstLibEntry<tMethod>("throw", &cHardwareGX::Inst_Throw),
    tInstLibEntry<tMethod>("throwif=0", &cHardwareGX::Inst_ThrowIf0),    
    tInstLibEntry<tMethod>("throwif!=0", &cHardwareGX::Inst_ThrowIfNot0),
    tInstLibEntry<tMethod>("catch", &cHardwareGX::Inst_Catch),
    
    tInstLibEntry<tMethod>("goto", &cHardwareGX::Inst_Goto),
    tInstLibEntry<tMethod>("goto-if=0", &cHardwareGX::Inst_GotoIf0),    
    tInstLibEntry<tMethod>("goto-if!=0", &cHardwareGX::Inst_GotoIfNot0),
    tInstLibEntry<tMethod>("label", &cHardwareGX::Inst_Label),
    
    tInstLibEntry<tMethod>("pop", &cHardwareGX::Inst_Pop, nInstFlag::DEFAULT, "Remove top number from stack and place into ?BX?"),
    tInstLibEntry<tMethod>("push", &cHardwareGX::Inst_Push, nInstFlag::DEFAULT, "Copy number from ?BX? and place it into the stack"),
    tInstLibEntry<tMethod>("swap-stk", &cHardwareGX::Inst_SwitchStack, nInstFlag::DEFAULT, "Toggle which stack is currently being used"),
    tInstLibEntry<tMethod>("flip-stk", &cHardwareGX::Inst_FlipStack),
    tInstLibEntry<tMethod>("swap", &cHardwareGX::Inst_Swap, nInstFlag::DEFAULT, "Swap the contents of ?BX? with ?CX?"),
    tInstLibEntry<tMethod>("swap-AB", &cHardwareGX::Inst_SwapAB),
    tInstLibEntry<tMethod>("swap-BC", &cHardwareGX::Inst_SwapBC),
    tInstLibEntry<tMethod>("swap-AC", &cHardwareGX::Inst_SwapAC),
    tInstLibEntry<tMethod>("copy-reg", &cHardwareGX::Inst_CopyReg),
    tInstLibEntry<tMethod>("set_A=B", &cHardwareGX::Inst_CopyRegAB),
    tInstLibEntry<tMethod>("set_A=C", &cHardwareGX::Inst_CopyRegAC),
    tInstLibEntry<tMethod>("set_B=A", &cHardwareGX::Inst_CopyRegBA),
    tInstLibEntry<tMethod>("set_B=C", &cHardwareGX::Inst_CopyRegBC),
    tInstLibEntry<tMethod>("set_C=A", &cHardwareGX::Inst_CopyRegCA),
    tInstLibEntry<tMethod>("set_C=B", &cHardwareGX::Inst_CopyRegCB),
    tInstLibEntry<tMethod>("reset", &cHardwareGX::Inst_Reset),
    
    tInstLibEntry<tMethod>("pop-A", &cHardwareGX::Inst_PopA),
    tInstLibEntry<tMethod>("pop-B", &cHardwareGX::Inst_PopB),
    tInstLibEntry<tMethod>("pop-C", &cHardwareGX::Inst_PopC),
    tInstLibEntry<tMethod>("push-A", &cHardwareGX::Inst_PushA),
    tInstLibEntry<tMethod>("push-B", &cHardwareGX::Inst_PushB),
    tInstLibEntry<tMethod>("push-C", &cHardwareGX::Inst_PushC),
    
    tInstLibEntry<tMethod>("shift-r", &cHardwareGX::Inst_ShiftR, nInstFlag::DEFAULT, "Shift bits in ?BX? right by one (divide by two)"),
    tInstLibEntry<tMethod>("shift-l", &cHardwareGX::Inst_ShiftL, nInstFlag::DEFAULT, "Shift bits in ?BX? left by one (multiply by two)"),
    tInstLibEntry<tMethod>("bit-1", &cHardwareGX::Inst_Bit1),
    tInstLibEntry<tMethod>("set-num", &cHardwareGX::Inst_SetNum),
    tInstLibEntry<tMethod>("val-grey", &cHardwareGX::Inst_ValGrey),
    tInstLibEntry<tMethod>("val-dir", &cHardwareGX::Inst_ValDir),
    tInstLibEntry<tMethod>("val-add-p", &cHardwareGX::Inst_ValAddP),
    tInstLibEntry<tMethod>("val-fib", &cHardwareGX::Inst_ValFib),
    tInstLibEntry<tMethod>("val-poly-c", &cHardwareGX::Inst_ValPolyC),
    tInstLibEntry<tMethod>("inc", &cHardwareGX::Inst_Inc, nInstFlag::DEFAULT, "Increment ?BX? by one"),
    tInstLibEntry<tMethod>("dec", &cHardwareGX::Inst_Dec, nInstFlag::DEFAULT, "Decrement ?BX? by one"),
    tInstLibEntry<tMethod>("zero", &cHardwareGX::Inst_Zero, 0, "Set ?BX? to zero"),
    tInstLibEntry<tMethod>("neg", &cHardwareGX::Inst_Neg),
    tInstLibEntry<tMethod>("square", &cHardwareGX::Inst_Square),
    tInstLibEntry<tMethod>("sqrt", &cHardwareGX::Inst_Sqrt),
    tInstLibEntry<tMethod>("not", &cHardwareGX::Inst_Not),
    
    tInstLibEntry<tMethod>("add", &cHardwareGX::Inst_Add, nInstFlag::DEFAULT, "Add BX to CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("sub", &cHardwareGX::Inst_Sub, nInstFlag::DEFAULT, "Subtract CX from BX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("mult", &cHardwareGX::Inst_Mult, 0, "Multiple BX by CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("div", &cHardwareGX::Inst_Div, 0, "Divide BX by CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("mod", &cHardwareGX::Inst_Mod),
    tInstLibEntry<tMethod>("nand", &cHardwareGX::Inst_Nand, nInstFlag::DEFAULT, "Nand BX by CX and place the result in ?BX?"),
    tInstLibEntry<tMethod>("nor", &cHardwareGX::Inst_Nor),
    tInstLibEntry<tMethod>("and", &cHardwareGX::Inst_And),
    tInstLibEntry<tMethod>("order", &cHardwareGX::Inst_Order),
    tInstLibEntry<tMethod>("xor", &cHardwareGX::Inst_Xor),
    
    tInstLibEntry<tMethod>("copy", &cHardwareGX::Inst_Copy),
    tInstLibEntry<tMethod>("read", &cHardwareGX::Inst_ReadInst),
    tInstLibEntry<tMethod>("write", &cHardwareGX::Inst_WriteInst),
    tInstLibEntry<tMethod>("stk-read", &cHardwareGX::Inst_StackReadInst),
    tInstLibEntry<tMethod>("stk-writ", &cHardwareGX::Inst_StackWriteInst),
    
    tInstLibEntry<tMethod>("compare", &cHardwareGX::Inst_Compare),
    tInstLibEntry<tMethod>("if-n-cpy", &cHardwareGX::Inst_IfNCpy),
    tInstLibEntry<tMethod>("allocate", &cHardwareGX::Inst_Allocate),
    tInstLibEntry<tMethod>("c-alloc", &cHardwareGX::Inst_CAlloc),
    tInstLibEntry<tMethod>("inject", &cHardwareGX::Inst_Inject),
    tInstLibEntry<tMethod>("inject-r", &cHardwareGX::Inst_InjectRand),
    tInstLibEntry<tMethod>("search-f", &cHardwareGX::Inst_SearchF),
    tInstLibEntry<tMethod>("search-b", &cHardwareGX::Inst_SearchB),
    tInstLibEntry<tMethod>("mem-size", &cHardwareGX::Inst_MemSize),
    
    tInstLibEntry<tMethod>("get", &cHardwareGX::Inst_TaskGet),
    tInstLibEntry<tMethod>("get-2", &cHardwareGX::Inst_TaskGet2),
    tInstLibEntry<tMethod>("stk-get", &cHardwareGX::Inst_TaskStackGet),
    tInstLibEntry<tMethod>("stk-load", &cHardwareGX::Inst_TaskStackLoad),
    tInstLibEntry<tMethod>("put", &cHardwareGX::Inst_TaskPut),
    tInstLibEntry<tMethod>("put-reset", &cHardwareGX::Inst_TaskPutResetInputs),
    tInstLibEntry<tMethod>("IO", &cHardwareGX::Inst_TaskIO, nInstFlag::DEFAULT, "Output ?BX?, and input new number back into ?BX?"),
    tInstLibEntry<tMethod>("IO-Feedback", &cHardwareGX::Inst_TaskIO_Feedback, 0, "Output ?BX?, and input new number back into ?BX?,  and push 1,0,  or -1 onto stack1 if merit increased, stayed the same, or decreased"),
    tInstLibEntry<tMethod>("match-strings", &cHardwareGX::Inst_MatchStrings),
    tInstLibEntry<tMethod>("sell", &cHardwareGX::Inst_Sell),
    tInstLibEntry<tMethod>("buy", &cHardwareGX::Inst_Buy),
    tInstLibEntry<tMethod>("send", &cHardwareGX::Inst_Send),
    tInstLibEntry<tMethod>("receive", &cHardwareGX::Inst_Receive),
    tInstLibEntry<tMethod>("sense", &cHardwareGX::Inst_SenseLog2),
    tInstLibEntry<tMethod>("sense-unit", &cHardwareGX::Inst_SenseUnit),
    tInstLibEntry<tMethod>("sense-m100", &cHardwareGX::Inst_SenseMult100),
    
    tInstLibEntry<tMethod>("donate-rnd", &cHardwareGX::Inst_DonateRandom),
    tInstLibEntry<tMethod>("donate-kin", &cHardwareGX::Inst_DonateKin),
    tInstLibEntry<tMethod>("donate-edt", &cHardwareGX::Inst_DonateEditDist),
    tInstLibEntry<tMethod>("donate-gbg",  &cHardwareGX::Inst_DonateGreenBeardGene),
    tInstLibEntry<tMethod>("donate-tgb",  &cHardwareGX::Inst_DonateTrueGreenBeard),
    tInstLibEntry<tMethod>("donate-threshgb",  &cHardwareGX::Inst_DonateThreshGreenBeard),
    tInstLibEntry<tMethod>("donate-quantagb",  &cHardwareGX::Inst_DonateQuantaThreshGreenBeard),
    tInstLibEntry<tMethod>("donate-NUL", &cHardwareGX::Inst_DonateNULL),
    
    tInstLibEntry<tMethod>("rotate-l", &cHardwareGX::Inst_RotateL),
    tInstLibEntry<tMethod>("rotate-r", &cHardwareGX::Inst_RotateR),
    
    tInstLibEntry<tMethod>("set-cmut", &cHardwareGX::Inst_SetCopyMut),
    tInstLibEntry<tMethod>("mod-cmut", &cHardwareGX::Inst_ModCopyMut),

    // Energy instruction
    tInstLibEntry<tMethod>("recover", &cHardwareGX::Inst_ZeroEnergyUsed),
        
    // Head-based instructions
    tInstLibEntry<tMethod>("h-alloc", &cHardwareGX::Inst_MaxAlloc, nInstFlag::DEFAULT, "Allocate maximum allowed space"),
    tInstLibEntry<tMethod>("h-divide", &cHardwareGX::Inst_HeadDivide, nInstFlag::DEFAULT, "Divide code between read and write heads."),
    tInstLibEntry<tMethod>("h-read", &cHardwareGX::Inst_HeadRead),
    tInstLibEntry<tMethod>("h-write", &cHardwareGX::Inst_HeadWrite),
    tInstLibEntry<tMethod>("h-copy", &cHardwareGX::Inst_HeadCopy, nInstFlag::DEFAULT, "Copy from read-head to write-head; advance both"),
    tInstLibEntry<tMethod>("h-search", &cHardwareGX::Inst_HeadSearch, nInstFlag::DEFAULT, "Find complement template and make with flow head"),
    tInstLibEntry<tMethod>("h-push", &cHardwareGX::Inst_HeadPush),
    tInstLibEntry<tMethod>("h-pop", &cHardwareGX::Inst_HeadPop),
    tInstLibEntry<tMethod>("adv-head", &cHardwareGX::Inst_AdvanceHead),
    tInstLibEntry<tMethod>("mov-head", &cHardwareGX::Inst_MoveHead, nInstFlag::DEFAULT, "Move head ?IP? to the flow head"),
    tInstLibEntry<tMethod>("jmp-head", &cHardwareGX::Inst_JumpHead, nInstFlag::DEFAULT, "Move head ?IP? by amount in CX register; CX = old pos."),
    tInstLibEntry<tMethod>("get-head", &cHardwareGX::Inst_GetHead, nInstFlag::DEFAULT, "Copy the position of the ?IP? head into CX"),
    tInstLibEntry<tMethod>("if-label", &cHardwareGX::Inst_IfLabel, nInstFlag::DEFAULT, "Execute next if we copied complement of attached label"),
    tInstLibEntry<tMethod>("if-label2", &cHardwareGX::Inst_IfLabel2, 0, "If copied label compl., exec next inst; else SKIP W/NOPS"),
    tInstLibEntry<tMethod>("set-flow", &cHardwareGX::Inst_SetFlow, nInstFlag::DEFAULT, "Set flow-head to position in ?CX?"),
    
    tInstLibEntry<tMethod>("h-copy2", &cHardwareGX::Inst_HeadCopy2),
    tInstLibEntry<tMethod>("h-copy3", &cHardwareGX::Inst_HeadCopy3),
    tInstLibEntry<tMethod>("h-copy4", &cHardwareGX::Inst_HeadCopy4),
    tInstLibEntry<tMethod>("h-copy5", &cHardwareGX::Inst_HeadCopy5),
    tInstLibEntry<tMethod>("h-copy6", &cHardwareGX::Inst_HeadCopy6),
    tInstLibEntry<tMethod>("h-copy7", &cHardwareGX::Inst_HeadCopy7),
    tInstLibEntry<tMethod>("h-copy8", &cHardwareGX::Inst_HeadCopy8),
    tInstLibEntry<tMethod>("h-copy9", &cHardwareGX::Inst_HeadCopy9),
    tInstLibEntry<tMethod>("h-copy10", &cHardwareGX::Inst_HeadCopy10),
        
    tInstLibEntry<tMethod>("spawn-deme", &cHardwareGX::Inst_SpawnDeme),
    
    // Suicide
    tInstLibEntry<tMethod>("kazi",	&cHardwareGX::Inst_Kazi),
    tInstLibEntry<tMethod>("kazi5", &cHardwareGX::Inst_Kazi5),
    tInstLibEntry<tMethod>("die", &cHardwareGX::Inst_Die),
	    
    // Placebo instructions
    tInstLibEntry<tMethod>("skip", &cHardwareGX::Inst_Skip),
    
    // Gene Expression Instructions
    tInstLibEntry<tMethod>("p-alloc", &cHardwareGX::Inst_NewExecutableProgramid),
    tInstLibEntry<tMethod>("g-alloc", &cHardwareGX::Inst_NewGenomeProgramid),
    tInstLibEntry<tMethod>("p-copy", &cHardwareGX::Inst_ProgramidCopy),
    tInstLibEntry<tMethod>("p-divide", &cHardwareGX::Inst_ProgramidDivide),
    
    tInstLibEntry<tMethod>("site", &cHardwareGX::Inst_Site),
    tInstLibEntry<tMethod>("bind", &cHardwareGX::Inst_Bind),
    tInstLibEntry<tMethod>("bind2", &cHardwareGX::Inst_Bind2),
    tInstLibEntry<tMethod>("if-bind", &cHardwareGX::Inst_IfBind),
    tInstLibEntry<tMethod>("if-bind2", &cHardwareGX::Inst_IfBind2),
    tInstLibEntry<tMethod>("num-sites", &cHardwareGX::Inst_NumSites),

    // These are dummy instructions used for making mutiple programids
    // look like one genome (for purposes of passing to offspring and printing).
    // They need to be included but should have ZERO probabilities!!!
    tInstLibEntry<tMethod>("PROGRAMID", &cHardwareGX::Inst_Nop),
    tInstLibEntry<tMethod>("READABLE", &cHardwareGX::Inst_Nop),
    tInstLibEntry<tMethod>("BINDABLE", &cHardwareGX::Inst_Nop),
    tInstLibEntry<tMethod>("EXECUTABLE", &cHardwareGX::Inst_Nop)
    
  };
  
  const int n_size = sizeof(s_n_array)/sizeof(cNOPEntryCPU);
  
  static cString n_names[n_size];
  static int nop_mods[n_size];
  for (int i = 0; i < n_size && i < NUM_REGISTERS; i++) {
    n_names[i] = s_n_array[i].name;
    nop_mods[i] = s_n_array[i].nop_mod;
  }
  
  const int f_size = sizeof(s_f_array)/sizeof(tInstLibEntry<tMethod>);
  static tMethod functions[f_size];
  for (int i = 0; i < f_size; i++) functions[i] = s_f_array[i].GetFunction();

	const cInstruction error(255);
	const cInstruction def(0);
  
  return new tInstLib<tMethod>(f_size, s_f_array, n_names, nop_mods, functions, error, def);
}


/*! Construct a cHardwareGX instance from the passed-in cOrganism.  This amounts to
creating an initial cProgramid from in_organism's genome.
*/
cHardwareGX::cHardwareGX(cWorld* world, cOrganism* in_organism, cInstSet* in_m_inst_set)
: cHardwareBase(world, in_organism, in_m_inst_set)
{
  m_last_unique_id_assigned = 0;
  m_functions = s_inst_slib->GetFunctions();
  Reset();   // Setup the rest of the hardware...also creates initial programid(s) from genome
}


/*! Destructor; delete all programids. */
cHardwareGX::~cHardwareGX() 
{
  std::for_each(m_programids.begin(), m_programids.end(), delete_functor());
}  


/*! Reset this cHardwareGX to a known state.  
Removes all the current cProgramids, and creates new cProgramids from the germ.
*/
void cHardwareGX::Reset()
{
  // Clear the current list of programids.
  std::for_each(m_programids.begin(), m_programids.end(), delete_functor());
  m_programids.clear(); 
 
  // And add any programids specified by the "genome."
  cGenome genome = organism->GetGenome();
  
  // These specify the range of instructions that will be used to create a new
  // programid.  The range of instructions used to create a programid is:
  // [begin, end), that is, the instruction pointed to by end is *not* copied.
  cInstruction* begin=&genome[0];
  cInstruction* end=&begin[genome.GetSize()];
  cInstruction* i=0;
  // Find the first instance of a PROGRAMID instruction.
  begin = std::find_if(begin, end, bind2nd(equal_to<cInstruction>(), GetInstSet().GetInst("PROGRAMID")));
  while(begin!=end) {
    // Find the boundary of this programid.
    i = std::find_if(begin+1, end, bind2nd(equal_to<cInstruction>(), GetInstSet().GetInst("PROGRAMID")));
    AddProgramid(new cProgramid(cGenome(begin, i), this));
    begin = i;
  }
  
  assert(m_programids.size()>0);  
  
  // Sanity, oh, where is my sanity?
  bool has_executable=false;
  bool has_bindable=false;
  for(programid_list::iterator i=m_programids.begin(); i!=m_programids.end(); ++i) {
    has_executable = has_executable || (*i)->GetExecutable();
    has_bindable = has_bindable || (*i)->GetBindable();
  }
  assert(has_bindable && has_executable);
  
  m_current = m_programids.back();
  m_mal_active = false;
  m_executedmatchstrings = false;
  
#if INSTRUCTION_COSTS
  // instruction cost arrays
  const int num_inst_cost = m_inst_set->GetSize();
  inst_cost.Resize(num_inst_cost);
  inst_ft_cost.Resize(num_inst_cost);
  
  for (int i = 0; i < num_inst_cost; i++) {
    inst_cost[i] = m_inst_set->GetCost(cInstruction(i));
    inst_ft_cost[i] = m_inst_set->GetFTCost(cInstruction(i));
  }
#endif
}


/*! In cHardwareGX, SingleProcess is something of a misnomer.  Each time this method
  is called, each cProgramid executes a single instruction.
  */
void cHardwareGX::SingleProcess(cAvidaContext& ctx)
{
  cPhenotype& phenotype = organism->GetPhenotype();

  organism->SetRunning(true);
  m_just_divided = false;
  phenotype.IncTimeUsed();

  // A temporary list of the current programids in this organism.
  programid_list runnable(m_programids.begin(), m_programids.end());

  // Execute one instruction for each programid.
  for(programid_list::iterator i=runnable.begin(); i!=runnable.end(); ++i) {
    // Currently executing programid.
    m_current = *i;
    
    // Print the status of this CPU at each step...
    if (m_tracer != NULL) m_tracer->TraceHardware(*this);
    
    if(m_current->GetExecute()) {
      // In case the IP is modified by this instruction, make sure that it wraps 
      // around to the beginning of the genome.
      IP().Adjust();
      
      // Find the instruction to be executed.
      const cInstruction& cur_inst = IP().GetInst();
      
      m_advance_ip = true;
      SingleProcess_ExecuteInst(ctx, cur_inst);
      
      // Break out if we just divided b/c the number of programids 
      // will have changed and it won't be obvious how to continue
      if (m_just_divided) break;
      if (m_advance_ip == true) { 
        IP().Advance();
      }
      
      // Update this programid stat
      m_current->IncCPUCyclesUsed();  
    }
  }
  m_current = 0;
  
  // Now kill old programids.
  unsigned int on_p = 0;
  while(on_p < m_programids.size()) {
    if(m_programids[on_p]->GetCPUCyclesUsed() > MAX_PROGRAMID_AGE) {
      RemoveProgramid(on_p);
    } else {
      on_p++;
    }
  }

  m_current = m_programids.back(); // m_current must always point to a programid!

  // Update phenotype. Note the difference between these cpu cycles and the per-programid ones...
  phenotype.IncCPUCyclesUsed(); 

  // Kill creatures who have reached their max num of instructions executed.
  const int max_executed = organism->GetMaxExecuted();
  if ((max_executed > 0 && phenotype.GetTimeUsed() >= max_executed)
      || phenotype.GetToDie() == true) {
    organism->Die();
  }  
  
  organism->SetRunning(false);
}

//  const int num_threads = GetNumThreads();
//  
//  // If we have threads turned on and we executed each thread in a single
//  // timestep, adjust the number of instructions executed accordingly.
//  const int num_inst_exec = (m_world->GetConfig().THREAD_SLICING_METHOD.Get() == 1) ?
//num_threads : 1;
//  
//  for (int i = 0; i < num_inst_exec; i++) {
//    // Setup the hardware for the next instruction to be executed.
//    ThreadNext();
//    m_advance_ip = true;
//    IP().Adjust();
//    
//#if BREAKPOINTS
//    if (IP().FlagBreakpoint()) {
//      organism->DoBreakpoint();
//    }
//#endif
//    
//    // Print the status of this CPU at each step...
//    if (m_tracer != NULL) m_tracer->TraceHardware(*this);
//    
//    // Find the instruction to be executed
//    const cInstruction& cur_inst = IP().GetInst();
//    
//    // Test if costs have been paid and it is okay to execute this now...
//    bool exec = SingleProcess_PayCosts(ctx, cur_inst);
//
//    // Now execute the instruction...
//    if (exec == true) {
//      // NOTE: This call based on the cur_inst must occur prior to instruction
//      //       execution, because this instruction reference may be invalid after
//      //       certain classes of instructions (namely divide instructions) @DMB
//      const int addl_time_cost = m_inst_set->GetAddlTimeCost(cur_inst);
//
//      // Prob of exec (moved from SingleProcess_PayCosts so that we advance IP after a fail)
//      if ( m_inst_set->GetProbFail(cur_inst) > 0.0 ) 
//      {
//        exec = !( ctx.GetRandom().P(m_inst_set->GetProbFail(cur_inst)) );
//      }
//      
//      if (exec == true) SingleProcess_ExecuteInst(ctx, cur_inst);
//      
//      // Some instruction (such as jump) may turn m_advance_ip off.  Usually
//      // we now want to move to the next instruction in the memory.
//      if (m_advance_ip == true) IP().Advance();
//      
//      // Pay the additional death_cost of the instruction now
//      phenotype.IncTimeUsed(addl_time_cost);
//    } // if exec
//    
//  } // Previous was executed once for each thread...
//  
//  // Kill creatures who have reached their max num of instructions executed
//  const int max_executed = organism->GetMaxExecuted();
//  if ((max_executed > 0 && phenotype.GetTimeUsed() >= max_executed)
//      || phenotype.GetToDie() == true) {
//    organism->Die();
//  }
//  
//  organism->SetRunning(false);
//}


// This method will test to see if all costs have been paid associated
// with executing an instruction and only return true when that instruction
// should proceed.
bool cHardwareGX::SingleProcess_PayCosts(cAvidaContext& ctx, const cInstruction& cur_inst)
{
#if INSTRUCTION_COSTS
  assert(cur_inst.GetOp() < inst_cost.GetSize());
  
  // If first time cost hasn't been paid off...
  if ( inst_ft_cost[cur_inst.GetOp()] > 0 ) {
    inst_ft_cost[cur_inst.GetOp()]--;       // dec cost
    return false;
  }
  
  // Next, look at the per use cost
  if ( m_inst_set->GetCost(cur_inst) > 0 ) {
    if ( inst_cost[cur_inst.GetOp()] > 1 ){  // if isn't paid off (>1)
      inst_cost[cur_inst.GetOp()]--;         // dec cost
      return false;
    } else {                                 // else, reset cost array
      inst_cost[cur_inst.GetOp()] = m_inst_set->GetCost(cur_inst);
    }
  }
  
#endif
  return true;
}


/*! This method executes one instruction for one programid. */
bool cHardwareGX::SingleProcess_ExecuteInst(cAvidaContext& ctx, const cInstruction& cur_inst) 
{
  // Copy the instruction in case of execution errors.
  cInstruction actual_inst = cur_inst;
  
#ifdef EXECUTION_ERRORS
  // If there is an execution error, execute a random instruction.
  if (organism->TestExeErr()) actual_inst = m_inst_set->GetRandomInst(ctx);
#endif /* EXECUTION_ERRORS */
  
  // Get the index for the instruction.
  int inst_idx = m_inst_set->GetLibFunctionIndex(actual_inst);
  
  // Mark the instruction as executed
  IP().SetFlagExecuted();
  
#if INSTRUCTION_COUNT
  // instruction execution count incremeneted
  organism->GetPhenotype().IncCurInstCount(actual_inst.GetOp());
#endif
	
  // And execute it.
  const bool exec_success = (this->*(m_functions[inst_idx]))(ctx);
	
#if INSTRUCTION_COUNT
  // decremenet if the instruction was not executed successfully
  if (exec_success == false) {
    organism->GetPhenotype().DecCurInstCount(actual_inst.GetOp());
  }
#endif	
  
  return exec_success;
}


void cHardwareGX::ProcessBonusInst(cAvidaContext& ctx, const cInstruction& inst)
{
  // Mark this organism as running...
  bool prev_run_state = organism->IsRunning();
  organism->SetRunning(true);
  
  if (m_tracer != NULL) m_tracer->TraceHardware(*this, true);
  
  SingleProcess_ExecuteInst(ctx, inst);
  
  organism->SetRunning(prev_run_state);
}


bool cHardwareGX::OK()
{
  bool result = true;
  for(programid_list::iterator i=m_programids.begin(); i!=m_programids.end() && result; ++i) {
    result = result && (*i)->m_memory.OK() && (*i)->m_stack.OK() && (*i)->m_nextLabel.OK();
  }
  return result;
}


/*! \todo Revisit.
*/
void cHardwareGX::PrintStatus(ostream& fp)
{
  //\todo clean up references
  
  // Extra information about this programid
  fp << "MemSpace:" << m_current->GetID() << "   Programid ID:" << m_current->m_unique_id;
  if (m_current->GetExecute()) fp << " EXECUTING";
  if (m_current->GetExecutable()) fp << " EXECUTABLE";
  if (m_current->GetBindable()) fp << " BINDABLE";
  if (m_current->GetReadable()) fp << " READABLE";
  fp << endl;

  fp << organism->GetPhenotype().GetCPUCyclesUsed() << " ";
  fp << "IP:" << IP().GetPosition() << "    ";
  
  for (int i = 0; i < NUM_REGISTERS; i++) {
    fp << static_cast<char>('A' + i) << "X:" << GetRegister(i) << " ";
    fp << setbase(16) << "[0x" << GetRegister(i) << "]  " << setbase(10);
  }
  fp << endl;
  
  fp << "  R-Head:" << GetHead(nHardware::HEAD_READ).GetMemSpace() << ":" << GetHead(nHardware::HEAD_READ).GetPosition() << " "
    << "W-Head:" << GetHead(nHardware::HEAD_WRITE).GetMemSpace() << ":" << GetHead(nHardware::HEAD_WRITE).GetPosition() << " "
    << "F-Head:" << GetHead(nHardware::HEAD_FLOW).GetMemSpace() << ":" << GetHead(nHardware::HEAD_FLOW).GetPosition() << "  "
    << "RL:" << GetReadLabel().AsString() << "   "
    << endl;

// This will have to be revisited soon.
//  int number_of_stacks = GetNumStacks();
//  for (int stack_id = 0; stack_id < number_of_stacks; stack_id++) {
//    fp << ((m_threads[m_cur_thread].cur_stack == stack_id) ? '*' : ' ') << " Stack " 
//          << stack_id << ":" << setbase(16) << setfill('0');
//    for (int i = 0; i < nHardware::STACK_SIZE; i++) 
//      fp << " Ox" << setw(8) << GetStack(i, stack_id, 0);
//    fp << setfill(' ') << setbase(10) << endl;
//  }

  fp << "  Mem (" << GetMemory().GetSize() << "):"
      << "  " << GetMemory().AsString()
      << endl;
  fp.flush();
}



/////////////////////////////////////////////////////////////////////////
// Method: cHardwareGX::FindLabel(direction)
//
// Search in 'direction' (+ or - 1) from the instruction pointer for the
// compliment of the label in 'next_label' and return a pointer to the
// results.  If direction is 0, search from the beginning of the genome.
//
/////////////////////////////////////////////////////////////////////////

cHeadCPU cHardwareGX::FindLabel(int direction)
{
  cHeadCPU & inst_ptr = IP();
  
  // Start up a search head at the position of the instruction pointer.
  cHeadCPU search_head(inst_ptr);
  cCodeLabel & search_label = GetLabel();
  
  // Make sure the label is of size > 0.
  
  if (search_label.GetSize() == 0) {
    return inst_ptr;
  }
  
  // Call special functions depending on if jump is forwards or backwards.
  int found_pos = 0;
  if( direction < 0 ) {
    found_pos = FindLabel_Backward(search_label, inst_ptr.GetMemory(),
                                   inst_ptr.GetPosition() - search_label.GetSize());
  }
  
  // Jump forward.
  else if (direction > 0) {
    found_pos = FindLabel_Forward(search_label, inst_ptr.GetMemory(),
                                  inst_ptr.GetPosition());
  }
  
  // Jump forward from the very beginning.
  else {
    found_pos = FindLabel_Forward(search_label, inst_ptr.GetMemory(), 0);
  }
  
  // Return the last line of the found label, if it was found.
  if (found_pos >= 0) search_head.Set(found_pos - 1);
  
  // Return the found position (still at start point if not found).
  return search_head;
}


// Search forwards for search_label from _after_ position pos in the
// memory.  Return the first line _after_ the the found label.  It is okay
// to find search label's match inside another label.

int cHardwareGX::FindLabel_Forward(const cCodeLabel & search_label,
                                    const cGenome & search_genome, int pos)
{
  assert (pos < search_genome.GetSize() && pos >= 0);
  
  int search_start = pos;
  int label_size = search_label.GetSize();
  bool found_label = false;
  
  // Move off the template we are on.
  pos += label_size;
  
  // Search until we find the complement or exit the memory.
  while (pos < search_genome.GetSize()) {
    
    // If we are within a label, rewind to the beginning of it and see if
    // it has the proper sub-label that we're looking for.
    
    if (m_inst_set->IsNop(search_genome[pos])) {
      // Find the start and end of the label we're in the middle of.
      
      int start_pos = pos;
      int end_pos = pos + 1;
      while (start_pos > search_start &&
             m_inst_set->IsNop( search_genome[start_pos - 1] )) {
        start_pos--;
      }
      while (end_pos < search_genome.GetSize() &&
             m_inst_set->IsNop( search_genome[end_pos] )) {
        end_pos++;
      }
      int test_size = end_pos - start_pos;
      
      // See if this label has the proper sub-label within it.
      int max_offset = test_size - label_size + 1;
      int offset = start_pos;
      for (offset = start_pos; offset < start_pos + max_offset; offset++) {
        
        // Test the number of matches for this offset.
        int matches;
        for (matches = 0; matches < label_size; matches++) {
          if (search_label[matches] !=
              m_inst_set->GetNopMod( search_genome[offset + matches] )) {
            break;
          }
        }
        
        // If we have found it, break out of this loop!
        if (matches == label_size) {
          found_label = true;
          break;
        }
      }
      
      // If we've found the complement label, set the position to the end of
      // the label we found it in, and break out.
      
      if (found_label == true) {
        // pos = end_pos;
        pos = label_size + offset;
        break;
      }
      
      // We haven't found it; jump pos to just after the current label being
      // checked.
      pos = end_pos;
    }
    
    // Jump up a block to the next possible point to find a label,
    pos += label_size;
  }
  
  // If the label was not found return a -1.
  if (found_label == false) pos = -1;
  
  return pos;
}

// Search backwards for search_label from _before_ position pos in the
// memory.  Return the first line _after_ the the found label.  It is okay
// to find search label's match inside another label.

int cHardwareGX::FindLabel_Backward(const cCodeLabel & search_label,
                                     const cGenome & search_genome, int pos)
{
  assert (pos < search_genome.GetSize());
  
  int search_start = pos;
  int label_size = search_label.GetSize();
  bool found_label = false;
  
  // Move off the template we are on.
  pos -= label_size;
  
  // Search until we find the complement or exit the memory.
  while (pos >= 0) {
    // If we are within a label, rewind to the beginning of it and see if
    // it has the proper sub-label that we're looking for.
    
    if (m_inst_set->IsNop( search_genome[pos] )) {
      // Find the start and end of the label we're in the middle of.
      
      int start_pos = pos;
      int end_pos = pos + 1;
      while (start_pos > 0 && m_inst_set->IsNop(search_genome[start_pos - 1])) {
        start_pos--;
      }
      while (end_pos < search_start &&
             m_inst_set->IsNop(search_genome[end_pos])) {
        end_pos++;
      }
      int test_size = end_pos - start_pos;
      
      // See if this label has the proper sub-label within it.
      int max_offset = test_size - label_size + 1;
      for (int offset = start_pos; offset < start_pos + max_offset; offset++) {
        
        // Test the number of matches for this offset.
        int matches;
        for (matches = 0; matches < label_size; matches++) {
          if (search_label[matches] !=
              m_inst_set->GetNopMod(search_genome[offset + matches])) {
            break;
          }
        }
        
        // If we have found it, break out of this loop!
        if (matches == label_size) {
          found_label = true;
          break;
        }
      }
      
      // If we've found the complement label, set the position to the end of
      // the label we found it in, and break out.
      
      if (found_label == true) {
        pos = end_pos;
        break;
      }
      
      // We haven't found it; jump pos to just before the current label
      // being checked.
      pos = start_pos - 1;
    }
    
    // Jump up a block to the next possible point to find a label,
    pos -= label_size;
  }
  
  // If the label was not found return a -1.
  if (found_label == false) pos = -1;
  
  return pos;
}

// Search for 'in_label' anywhere in the hardware.
cHeadCPU cHardwareGX::FindLabel(const cCodeLabel & in_label, int direction)
{
  assert (in_label.GetSize() > 0);
  
  // IDEALY:
  // Keep making jumps (in the proper direction) equal to the label
  // length.  If we are inside of a label, check its size, and see if
  // any of the sub-labels match properly.
  // FOR NOW:
  // Get something which works, no matter how inefficient!!!
  
  cHeadCPU temp_head(this);
  
  while (temp_head.InMemory()) {
    // IDEALY: Analyze the label we are in; see if the one we are looking
    // for could be a sub-label of it.  Skip past it if not.
    
    int i;
    for (i = 0; i < in_label.GetSize(); i++) {
      if (!m_inst_set->IsNop(temp_head.GetInst()) ||
          in_label[i] != m_inst_set->GetNopMod(temp_head.GetInst())) {
        break;
      }
    }
    if (i == GetLabel().GetSize()) {
      temp_head.AbsJump(i - 1);
      return temp_head;
    }
    
    temp_head.AbsJump(direction);     // IDEALY: MAKE LARGER JUMPS
  }
  
  temp_head.AbsSet(-1);
  return temp_head;
}


/*! Inject a genome fragment into this CPU.  This works a little differently in
cHardwareGX, in that we don't insert a genome fragment into a preexisting genome,
but instead ust create a new cProgramid with the genome-to-be-injected.
*/
bool cHardwareGX::InjectHost(const cCodeLabel & in_label, const cGenome & injection)
{
  InjectCode(injection, -1);
  return true;
}


/*! Inject a genome fragment into this CPU.  This works differently in 
cHardwareGX -- We just insert a new cProgramid.
*/
void cHardwareGX::InjectCode(const cGenome & inject_code, const int line_num)
{
  programid_ptr injected = new cProgramid(inject_code, this);

  // Set instruction flags on the injected code
  for(int i=0; i<injected->m_memory.GetSize(); ++i) {
    injected->m_memory.SetFlagInjected(i);
  }

  AddProgramid(injected);
  
  organism->GetPhenotype().IsModified() = true;
}


void cHardwareGX::ReadInst(const int in_inst)
{
  if(m_inst_set->IsNop(cInstruction(in_inst))) {
    GetReadLabel().AddNop(in_inst);
  } else {
    GetReadLabel().Clear();
  }
}


void cHardwareGX::AdjustHeads()
{
//  for (int i = 0; i < GetNumThreads(); i++) {
//    for (int j = 0; j < NUM_HEADS; j++) {
//      m_threads[i].heads[j].Adjust();
//    }
//  }
}



// This function looks at the current position in the info of a creature,
// and sets the next_label to be the sequence of nops which follows.  The
// instruction pointer is left on the last line of the label found.

void cHardwareGX::ReadLabel(int max_size)
{
  int count = 0;
  cHeadCPU * inst_ptr = &( IP() );
  
  GetLabel().Clear();
  
  while (m_inst_set->IsNop(inst_ptr->GetNextInst()) &&
         (count < max_size)) {
    count++;
    inst_ptr->Advance();
    GetLabel().AddNop(m_inst_set->GetNopMod(inst_ptr->GetInst()));
    
    // If this is the first line of the template, mark it executed.
    if (GetLabel().GetSize() <=	m_world->GetConfig().MAX_LABEL_EXE_SIZE.Get()) {
      inst_ptr->SetFlagExecuted();
    }
  }
}


//bool cHardwareGX::ForkThread()
//{
//  const int num_threads = GetNumThreads();
//  if (num_threads == m_world->GetConfig().MAX_CPU_THREADS.Get()) return false;
//  
//  // Make room for the new thread.
//  m_threads.Resize(num_threads + 1);
//  
//  // Initialize the new thread to the same values as the current one.
//  m_threads[num_threads] = m_threads[m_cur_thread];
//  
//  // Find the first free bit in m_thread_id_chart to determine the new
//  // thread id.
//  int new_id = 0;
//  while ( (m_thread_id_chart >> new_id) & 1 == 1) new_id++;
//  m_threads[num_threads].SetID(new_id);
//  m_thread_id_chart |= (1 << new_id);
//  
//  return true;
//}
//
//
//bool cHardwareGX::KillThread()
//{
//  // Make sure that there is always at least one thread...
//  if (GetNumThreads() == 1) return false;
//  
//  // Note the current thread and set the current back one.
//  const int kill_thread = m_cur_thread;
//  ThreadPrev();
//  
//  // Turn off this bit in the m_thread_id_chart...
//  m_thread_id_chart ^= 1 << m_threads[kill_thread].GetID();
//  
//  // Copy the last thread into the kill position
//  const int last_thread = GetNumThreads() - 1;
//  if (last_thread != kill_thread) {
//    m_threads[kill_thread] = m_threads[last_thread];
//  }
//  
//  // Kill the thread!
//  m_threads.Resize(GetNumThreads() - 1);
//  
//  if (m_cur_thread > kill_thread) m_cur_thread--;
//  
//  return true;
//}

////////////////////////////
//  Instruction Helpers...
////////////////////////////

inline int cHardwareGX::FindModifiedRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_register = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  }
  return default_register;
}

inline int cHardwareGX::FindModifiedNextRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_register = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  } else {
    default_register = (default_register + 1) % NUM_REGISTERS;
  }
  return default_register;
}

inline int cHardwareGX::FindModifiedPreviousRegister(int default_register)
{
  assert(default_register < NUM_REGISTERS);  // Reg ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_register = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  } else {
    default_register = (default_register + NUM_REGISTERS - 1) % NUM_REGISTERS;
  }
  return default_register;
}


inline int cHardwareGX::FindModifiedHead(int default_head)
{
  assert(default_head < NUM_HEADS); // Head ID too high.
  
  if (m_inst_set->IsNop(IP().GetNextInst())) {
    IP().Advance();
    default_head = m_inst_set->GetNopMod(IP().GetInst());
    IP().SetFlagExecuted();
  }
  return default_head;
}


inline int cHardwareGX::FindNextRegister(int base_reg)
{
  return (base_reg + 1) % NUM_REGISTERS;
}


bool cHardwareGX::Allocate_Necro(const int new_size)
{
  GetMemory().ResizeOld(new_size);
  return true;
}

bool cHardwareGX::Allocate_Random(cAvidaContext& ctx, const int old_size, const int new_size)
{
  GetMemory().Resize(new_size);
  
  for (int i = old_size; i < new_size; i++) {
    GetMemory()[i] = m_inst_set->GetRandomInst(ctx);
  }
  return true;
}

bool cHardwareGX::Allocate_Default(const int new_size)
{
  GetMemory().Resize(new_size);
  
  // New space already defaults to default instruction...
  
  return true;
}

bool cHardwareGX::Allocate_Main(cAvidaContext& ctx, const int allocated_size)
{
  // must do divide before second allocate & must allocate positive amount...
  if (m_world->GetConfig().REQUIRE_ALLOCATE.Get() && m_mal_active == true) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR, "Allocate already active");
    return false;
  }
  if (allocated_size < 1) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
          cStringUtil::Stringf("Allocate of %d too small", allocated_size));
    return false;
  }
  
  const int old_size = GetMemory().GetSize();
  const int new_size = old_size + allocated_size;
  
  // Make sure that the new size is in range.
  if (new_size > MAX_CREATURE_SIZE  ||  new_size < MIN_CREATURE_SIZE) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
          cStringUtil::Stringf("Invalid post-allocate size (%d)",
                               new_size));
    return false;
  }
  
  const int max_alloc_size = (int) (old_size * m_world->GetConfig().CHILD_SIZE_RANGE.Get());
  if (allocated_size > max_alloc_size) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
          cStringUtil::Stringf("Allocate too large (%d > %d)",
                               allocated_size, max_alloc_size));
    return false;
  }
  
  const int max_old_size =
    (int) (allocated_size * m_world->GetConfig().CHILD_SIZE_RANGE.Get());
  if (old_size > max_old_size) {
    organism->Fault(FAULT_LOC_ALLOC, FAULT_TYPE_ERROR,
          cStringUtil::Stringf("Allocate too small (%d > %d)",
                               old_size, max_old_size));
    return false;
  }
  
  switch (m_world->GetConfig().ALLOC_METHOD.Get()) {
    case ALLOC_METHOD_NECRO:
      // Only break if this succeeds -- otherwise just do random.
      if (Allocate_Necro(new_size) == true) break;
    case ALLOC_METHOD_RANDOM:
      Allocate_Random(ctx, old_size, new_size);
      break;
    case ALLOC_METHOD_DEFAULT:
      Allocate_Default(new_size);
      break;
  }
  
  m_mal_active = true;
  
  return true;
}

int cHardwareGX::GetCopiedSize(const int parent_size, const int child_size)
{
  int copied_size = 0;
  const cCPUMemory& memory = GetMemory();
  for (int i = parent_size; i < parent_size + child_size; i++) {
    if (memory.FlagCopied(i)) copied_size++;
  }
  return copied_size;
}  


/*! Divide works a little differently in cHardwareGX than in other CPUs.  First,
the cProgramid directly builds its offspring (rather than attaching its genome), so
we don't need to do any genome splitting.  This also enables the offspring to be
active during replication.  Second, we have to divvy up the other cProgramid objects 
between the parent and offspring.  We also have to be careful to make sure that
we have two genomes!
*/
bool cHardwareGX::Divide_Main(cAvidaContext& ctx)
{
  
//  if(m_current->TriggerReplication()) {
//  } else {
//  }
//  
  
//  const int child_size = GetMemory().GetSize() - div_point - extra_lines;
//  
//  // Make sure this divide will produce a viable offspring.
//  const bool viable = Divide_CheckViable(ctx, div_point, child_size);
//  if (viable == false) return false;
//  
//  // Since the divide will now succeed, set up the information to be sent
//  // to the new organism
//  cGenome & child_genome = organism->ChildGenome();
//  child_genome = cGenomeUtil::Crop(m_memory, div_point, div_point+child_size);
//  
//  // Cut off everything in this memory past the divide point.
//  GetMemory().Resize(div_point);
//  
//  // Handle Divide Mutations...
//  Divide_DoMutations(ctx, mut_multiplier);
//  
//  // Many tests will require us to run the offspring through a test CPU;
//  // this is, for example, to see if mutations need to be reverted or if
//  // lineages need to be updated.
//  Divide_TestFitnessMeasures(ctx);
//  
//#if INSTRUCTION_COSTS
//  // reset first time instruction costs
//  for (int i = 0; i < inst_ft_cost.GetSize(); i++) {
//    inst_ft_cost[i] = m_inst_set->GetFTCost(cInstruction(i));
//  }
//#endif
//  
//  m_mal_active = false;
//  if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) {
//    m_advance_ip = false;
//  }
//  
//  // Activate the child
//  bool parent_alive = organism->ActivateDivide(ctx);
//
//  // Do more work if the parent lives through the birth of the offspring
//  if (parent_alive) {
//    if (m_world->GetConfig().DIVIDE_METHOD.Get() == DIVIDE_METHOD_SPLIT) Reset();
//  }
//  
  return true;
}


//////////////////////////
// And the instructions...
//////////////////////////

bool cHardwareGX::Inst_If0(cAvidaContext& ctx)          // Execute next if ?bx? ==0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) != 0)  IP().Advance();
  return true; 
}

bool cHardwareGX::Inst_IfNot0(cAvidaContext& ctx)       // Execute next if ?bx? != 0.
{ 
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) == 0)  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfEqu(cAvidaContext& ctx)      // Execute next if bx == ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) != GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfNEqu(cAvidaContext& ctx)     // Execute next if bx != ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) == GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfGr0(cAvidaContext& ctx)       // Execute next if ?bx? ! < 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) <= 0)  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfGr(cAvidaContext& ctx)       // Execute next if bx > ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) <= GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfGrEqu0(cAvidaContext& ctx)       // Execute next if ?bx? != 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) < 0)  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfGrEqu(cAvidaContext& ctx)       // Execute next if bx > ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) < GetRegister(op2)) IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfLess0(cAvidaContext& ctx)       // Execute next if ?bx? != 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) >= 0)  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfLess(cAvidaContext& ctx)       // Execute next if ?bx? < ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) >=  GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfLsEqu0(cAvidaContext& ctx)       // Execute next if ?bx? != 0.
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if (GetRegister(reg_used) > 0) IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfLsEqu(cAvidaContext& ctx)       // Execute next if bx > ?cx?
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  if (GetRegister(op1) >  GetRegister(op2))  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfBit1(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  if ((GetRegister(reg_used) & 1) == 0)  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfANotEqB(cAvidaContext& ctx)     // Execute next if AX != BX
{
  if (GetRegister(REG_AX) == GetRegister(REG_BX) )  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfBNotEqC(cAvidaContext& ctx)     // Execute next if BX != CX
{
  if (GetRegister(REG_BX) == GetRegister(REG_CX) )  IP().Advance();
  return true;
}

bool cHardwareGX::Inst_IfANotEqC(cAvidaContext& ctx)     // Execute next if AX != BX
{
  if (GetRegister(REG_AX) == GetRegister(REG_CX) )  IP().Advance();
  return true;
}


bool cHardwareGX::Inst_Call(cAvidaContext& ctx)
{
  // Put the starting location onto the stack
  const int location = IP().GetPosition();
  StackPush(location);
  
  // Jump to the compliment label (or by the ammount in the bx register)
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  
  if (GetLabel().GetSize() == 0) {
    IP().Jump(GetRegister(REG_BX));
    return true;
  }
  
  const cHeadCPU jump_location(FindLabel(1));
  if (jump_location.GetPosition() != -1) {
    IP().Set(jump_location);
    return true;
  }
  
  // If complement label was not found; record an error.
  organism->Fault(FAULT_LOC_JUMP, FAULT_TYPE_ERROR,
                  "call: no complement label");
  return false;
}

bool cHardwareGX::Inst_Return(cAvidaContext& ctx)
{
  IP().Set(StackPop());
  return true;
}

bool cHardwareGX::Inst_Throw(cAvidaContext& ctx)
{
  // Only initialize this once to save some time...
  static cInstruction catch_inst = GetInstSet().GetInst(cStringUtil::Stringf("catch"));

  //Look for the label directly (no complement)
  ReadLabel();
    
  cHeadCPU search_head(IP());
  int start_pos = search_head.GetPosition();
  search_head++;
  
  while (start_pos != search_head.GetPosition()) 
  {
    // If we find a catch instruction, compare the NOPs following it
    if (search_head.GetInst() == catch_inst)
    {
      int catch_pos = search_head.GetPosition();
      search_head++;

      // Continue to examine the label after the catch
      //  (1) It ends (=> use the catch!)
      //  (2) It becomes longer than the throw label (=> use the catch!)
      //  (3) We find a NOP that doesnt match the throw (=> DON'T use the catch...)
      
      bool match = true;
      int size_matched = 0;      
      while ( match && m_inst_set->IsNop(search_head.GetInst()) && (size_matched < GetLabel().GetSize()) )
      {
        if ( GetLabel()[size_matched] != m_inst_set->GetNopMod( search_head.GetInst()) ) match = false;
        search_head++;
        size_matched++;
      }
      
      // We found a matching catch instruction
      if (match)
      {
        IP().Set(catch_pos);
        m_advance_ip = false; // Don't automatically move the IP
                              // so we mark the catch as executed.
        return true;
      }
      
      //If we advanced past NOPs during testing, retreat
      if ( !m_inst_set->IsNop(search_head.GetInst()) ) search_head--;
    }
    search_head.Advance();
  }

  return false;
}


bool cHardwareGX::Inst_ThrowIfNot0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) == 0) return false;
  return Inst_Throw(ctx);
}

bool cHardwareGX::Inst_ThrowIf0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) != 0) return false;
  return Inst_Throw(ctx);
}

bool cHardwareGX::Inst_Goto(cAvidaContext& ctx)
{
  // Only initialize this once to save some time...
  static cInstruction label_inst = GetInstSet().GetInst(cStringUtil::Stringf("label"));

  //Look for an EXACT label match after a 'label' instruction
  ReadLabel();
  
  cHeadCPU search_head(IP());
  int start_pos = search_head.GetPosition();
  search_head++;
  
  while (start_pos != search_head.GetPosition()) 
  {
    if (search_head.GetInst() == label_inst)
    {
      int label_pos = search_head.GetPosition();
      search_head++;
      int size_matched = 0;
      while ( size_matched < GetLabel().GetSize() )
      {
        if ( !m_inst_set->IsNop(search_head.GetInst()) ) break;
        if ( GetLabel()[size_matched] != m_inst_set->GetNopMod( search_head.GetInst()) ) break;
        if ( !m_inst_set->IsNop(search_head.GetInst()) ) break;

        size_matched++;
        search_head++;
      }
      
      // We found a matching 'label' instruction only if the next 
      // instruction (at the search head now) is also not a NOP
      if ( (size_matched == GetLabel().GetSize()) && !m_inst_set->IsNop(search_head.GetInst()) )
      {
        IP().Set(label_pos);
        m_advance_ip = false; // Don't automatically move the IP
                              // so we mark the catch as executed.
        return true;
      }

      //If we advanced past NOPs during testing, retreat
      if ( !m_inst_set->IsNop(search_head.GetInst()) ) search_head--;
    }
    search_head++;
  }

  return false;
}


bool cHardwareGX::Inst_GotoIfNot0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) == 0) return false;
  return Inst_Goto(ctx);
}

bool cHardwareGX::Inst_GotoIf0(cAvidaContext& ctx)
{
  if (GetRegister(REG_BX) != 0) return false;
  return Inst_Goto(ctx);
}


bool cHardwareGX::Inst_Pop(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = StackPop();
  return true;
}

bool cHardwareGX::Inst_Push(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  StackPush(GetRegister(reg_used));
  return true;
}

bool cHardwareGX::Inst_HeadPop(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  GetHead(head_used).Set(StackPop());
  return true;
}

bool cHardwareGX::Inst_HeadPush(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  StackPush(GetHead(head_used).GetPosition());
  if (head_used == nHardware::HEAD_IP) {
    GetHead(head_used).Set(GetHead(nHardware::HEAD_FLOW));
    m_advance_ip = false;
  }
  return true;
}


bool cHardwareGX::Inst_PopA(cAvidaContext& ctx) { GetRegister(REG_AX) = StackPop(); return true;}
bool cHardwareGX::Inst_PopB(cAvidaContext& ctx) { GetRegister(REG_BX) = StackPop(); return true;}
bool cHardwareGX::Inst_PopC(cAvidaContext& ctx) { GetRegister(REG_CX) = StackPop(); return true;}

bool cHardwareGX::Inst_PushA(cAvidaContext& ctx) { StackPush(GetRegister(REG_AX)); return true;}
bool cHardwareGX::Inst_PushB(cAvidaContext& ctx) { StackPush(GetRegister(REG_BX)); return true;}
bool cHardwareGX::Inst_PushC(cAvidaContext& ctx) { StackPush(GetRegister(REG_CX)); return true;}

bool cHardwareGX::Inst_SwitchStack(cAvidaContext& ctx) { SwitchStack(); return true;}
bool cHardwareGX::Inst_FlipStack(cAvidaContext& ctx)   { StackFlip(); return true;}

bool cHardwareGX::Inst_Swap(cAvidaContext& ctx)
{
  const int op1 = FindModifiedRegister(REG_BX);
  const int op2 = FindNextRegister(op1);
  nFunctions::Swap(GetRegister(op1), GetRegister(op2));
  return true;
}

bool cHardwareGX::Inst_SwapAB(cAvidaContext& ctx)\
{
  nFunctions::Swap(GetRegister(REG_AX), GetRegister(REG_BX)); return true;
}
bool cHardwareGX::Inst_SwapBC(cAvidaContext& ctx)
{
  nFunctions::Swap(GetRegister(REG_BX), GetRegister(REG_CX)); return true;
}
bool cHardwareGX::Inst_SwapAC(cAvidaContext& ctx)
{
  nFunctions::Swap(GetRegister(REG_AX), GetRegister(REG_CX)); return true;
}

bool cHardwareGX::Inst_CopyReg(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = FindNextRegister(src);
  GetRegister(dst) = GetRegister(src);
  return true;
}

bool cHardwareGX::Inst_CopyRegAB(cAvidaContext& ctx)
{
  GetRegister(REG_AX) = GetRegister(REG_BX);   return true;
}
bool cHardwareGX::Inst_CopyRegAC(cAvidaContext& ctx)
{
  GetRegister(REG_AX) = GetRegister(REG_CX);   return true;
}
bool cHardwareGX::Inst_CopyRegBA(cAvidaContext& ctx)
{
  GetRegister(REG_BX) = GetRegister(REG_AX);   return true;
}
bool cHardwareGX::Inst_CopyRegBC(cAvidaContext& ctx)
{
  GetRegister(REG_BX) = GetRegister(REG_CX);   return true;
}
bool cHardwareGX::Inst_CopyRegCA(cAvidaContext& ctx)
{
  GetRegister(REG_CX) = GetRegister(REG_AX);   return true;
}
bool cHardwareGX::Inst_CopyRegCB(cAvidaContext& ctx)
{
  GetRegister(REG_CX) = GetRegister(REG_BX);   return true;
}

bool cHardwareGX::Inst_Reset(cAvidaContext& ctx)
{
  GetRegister(REG_AX) = 0;
  GetRegister(REG_BX) = 0;
  GetRegister(REG_CX) = 0;
  StackClear();
  return true;
}

bool cHardwareGX::Inst_ShiftR(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) >>= 1;
  return true;
}

bool cHardwareGX::Inst_ShiftL(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) <<= 1;
  return true;
}

bool cHardwareGX::Inst_Bit1(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) |=  1;
  return true;
}

bool cHardwareGX::Inst_SetNum(cAvidaContext& ctx)
{
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsInt(NUM_NOPS);
  return true;
}

bool cHardwareGX::Inst_ValGrey(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntGreyCode(NUM_NOPS);
  return true;
}

bool cHardwareGX::Inst_ValDir(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntDirect(NUM_NOPS);
  return true;
}

bool cHardwareGX::Inst_ValAddP(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntAdditivePolynomial(NUM_NOPS);
  return true;
}

bool cHardwareGX::Inst_ValFib(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntFib(NUM_NOPS);
  return true;
}

bool cHardwareGX::Inst_ValPolyC(cAvidaContext& ctx) {
  ReadLabel();
  GetRegister(REG_BX) = GetLabel().AsIntPolynomialCoefficent(NUM_NOPS);
  return true;
}

bool cHardwareGX::Inst_Inc(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) += 1;
  return true;
}

bool cHardwareGX::Inst_Dec(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) -= 1;
  return true;
}

bool cHardwareGX::Inst_Zero(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareGX::Inst_Neg(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  GetRegister(dst) = -GetRegister(src);
  return true;
}

bool cHardwareGX::Inst_Square(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  GetRegister(dst) = GetRegister(src) * GetRegister(src);
  return true;
}

bool cHardwareGX::Inst_Sqrt(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  const int value = GetRegister(src);
  if (value > 1) GetRegister(dst) = static_cast<int>(sqrt(static_cast<double>(value)));
  else if (value < 0) {
    organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "sqrt: value is negative");
    return false;
  }
  return true;
}

bool cHardwareGX::Inst_Log(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  const int value = GetRegister(src);
  if (value >= 1) GetRegister(dst) = static_cast<int>(log(static_cast<double>(value)));
  else if (value < 0) {
    organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "log: value is negative");
    return false;
  }
  return true;
}

bool cHardwareGX::Inst_Log10(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  const int value = GetRegister(src);
  if (value >= 1) GetRegister(dst) = static_cast<int>(log10(static_cast<double>(value)));
  else if (value < 0) {
    organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "log10: value is negative");
    return false;
  }
  return true;
}

bool cHardwareGX::Inst_Add(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) + GetRegister(op2);
  return true;
}

bool cHardwareGX::Inst_Sub(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) - GetRegister(op2);
  return true;
}

bool cHardwareGX::Inst_Mult(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) * GetRegister(op2);
  return true;
}

bool cHardwareGX::Inst_Div(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  if (GetRegister(op2) != 0) {
    if (0-INT_MAX > GetRegister(op1) && GetRegister(op2) == -1)
      organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "div: Float exception");
    else
      GetRegister(dst) = GetRegister(op1) / GetRegister(op2);
  } else {
    organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "div: dividing by 0");
    return false;
  }
  return true;
}

bool cHardwareGX::Inst_Mod(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  if (GetRegister(op2) != 0) {
    GetRegister(dst) = GetRegister(op1) % GetRegister(op2);
  } else {
    organism->Fault(FAULT_LOC_MATH, FAULT_TYPE_ERROR, "mod: modding by 0");
    return false;
  }
  return true;
}


bool cHardwareGX::Inst_Nand(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = ~(GetRegister(op1) & GetRegister(op2));
  return true;
}

bool cHardwareGX::Inst_Nor(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = ~(GetRegister(op1) | GetRegister(op2));
  return true;
}

bool cHardwareGX::Inst_And(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = (GetRegister(op1) & GetRegister(op2));
  return true;
}

bool cHardwareGX::Inst_Not(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_BX);
  const int dst = src;
  GetRegister(dst) = ~(GetRegister(src));
  return true;
}

bool cHardwareGX::Inst_Order(cAvidaContext& ctx)
{
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  if (GetRegister(op1) > GetRegister(op2)) {
    nFunctions::Swap(GetRegister(op1), GetRegister(op2));
  }
  return true;
}

bool cHardwareGX::Inst_Xor(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_BX;
  const int op2 = REG_CX;
  GetRegister(dst) = GetRegister(op1) ^ GetRegister(op2);
  return true;
}

bool cHardwareGX::Inst_Copy(cAvidaContext& ctx)
{
  const int op1 = REG_BX;
  const int op2 = REG_AX;

  const cHeadCPU from(this, GetRegister(op1));
  cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  sCPUStats& cpu_stats = organism->CPUStats();
  
  if (organism->TestCopyMut(ctx)) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();  // Mark this instruction as mutated...
    to.SetFlagCopyMut();  // Mark this instruction as copy mut...
                              //organism->GetPhenotype().IsMutated() = true;
    cpu_stats.mut_stats.copy_mut_count++;
  } else {
    to.SetInst(from.GetInst());
    to.ClearFlagMutated();  // UnMark
    to.ClearFlagCopyMut();  // UnMark
  }
  
  to.SetFlagCopied();  // Set the copied flag.
  cpu_stats.mut_stats.copies_exec++;
  return true;
}

bool cHardwareGX::Inst_ReadInst(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_CX);
  const int src = REG_BX;

  const cHeadCPU from(this, GetRegister(src));
  
  // Dis-allowing mutations on read, for the moment (write only...)
  // @CAO This allows perfect error-correction...
  GetRegister(dst) = from.GetInst().GetOp();
  return true;
}

bool cHardwareGX::Inst_WriteInst(cAvidaContext& ctx)
{
  const int src = FindModifiedRegister(REG_CX);
  const int op1 = REG_BX;
  const int op2 = REG_AX;

  cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  const int value = Mod(GetRegister(src), m_inst_set->GetSize());
  sCPUStats& cpu_stats = organism->CPUStats();

  // Change value on a mutation...
  if (organism->TestCopyMut(ctx)) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();      // Mark this instruction as mutated...
    to.SetFlagCopyMut();      // Mark this instruction as copy mut...
                                  //organism->GetPhenotype().IsMutated() = true;
    cpu_stats.mut_stats.copy_mut_count++;
  } else {
    to.SetInst(cInstruction(value));
    to.ClearFlagMutated();     // UnMark
    to.ClearFlagCopyMut();     // UnMark
  }

  to.SetFlagCopied();  // Set the copied flag.
  cpu_stats.mut_stats.copies_exec++;
  return true;
}

bool cHardwareGX::Inst_StackReadInst(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  cHeadCPU from(this, GetRegister(reg_used));
  StackPush(from.GetInst().GetOp());
  return true;
}

bool cHardwareGX::Inst_StackWriteInst(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_BX);
  const int op1 = REG_AX;
  cHeadCPU to(this, GetRegister(op1) + GetRegister(dst));
  const int value = Mod(StackPop(), m_inst_set->GetSize());
  sCPUStats& cpu_stats = organism->CPUStats();
  
  // Change value on a mutation...
  if (organism->TestCopyMut(ctx)) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();      // Mark this instruction as mutated...
    to.SetFlagCopyMut();      // Mark this instruction as copy mut...
                                  //organism->GetPhenotype().IsMutated() = true;
    cpu_stats.mut_stats.copy_mut_count++;
  } else {
    to.SetInst(cInstruction(value));
    to.ClearFlagMutated();     // UnMark
    to.ClearFlagCopyMut();     // UnMark
  }
  
  to.SetFlagCopied();  // Set the copied flag.
  cpu_stats.mut_stats.copies_exec++;
  return true;
}

bool cHardwareGX::Inst_Compare(cAvidaContext& ctx)
{
  const int dst = FindModifiedRegister(REG_CX);
  const int op1 = REG_BX;
  const int op2 = REG_AX;

  cHeadCPU from(this, GetRegister(op1));
  cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  
  // Compare is dangerous -- it can cause mutations!
  if (organism->TestCopyMut(ctx)) {
    to.SetInst(m_inst_set->GetRandomInst(ctx));
    to.SetFlagMutated();      // Mark this instruction as mutated...
    to.SetFlagCopyMut();      // Mark this instruction as copy mut...
                                  //organism->GetPhenotype().IsMutated() = true;
  }
  
  GetRegister(dst) = from.GetInst().GetOp() - to.GetInst().GetOp();
  
  return true;
}

bool cHardwareGX::Inst_IfNCpy(cAvidaContext& ctx)
{
  const int op1 = REG_BX;
  const int op2 = REG_AX;

  const cHeadCPU from(this, GetRegister(op1));
  const cHeadCPU to(this, GetRegister(op2) + GetRegister(op1));
  
  // Allow for errors in this test...
  if (organism->TestCopyMut(ctx)) {
    if (from.GetInst() != to.GetInst()) IP().Advance();
  } else {
    if (from.GetInst() == to.GetInst()) IP().Advance();
  }
  return true;
}

bool cHardwareGX::Inst_Allocate(cAvidaContext& ctx)   // Allocate bx more space...
{
  const int src = REG_BX;
  const int dst = REG_AX;
  const int size = GetMemory().GetSize();
  if (Allocate_Main(ctx, GetRegister(src))) {
    GetRegister(dst) = size;
    return true;
  } else return false;
}


bool cHardwareGX::Inst_CAlloc(cAvidaContext& ctx)  
{ 
  return Allocate_Main(ctx, GetMemory().GetSize());   
}

bool cHardwareGX::Inst_MaxAlloc(cAvidaContext& ctx)   // Allocate maximal more
{
  const int dst = REG_AX;
  const int cur_size = GetMemory().GetSize();
  const int alloc_size = Min((int) (m_world->GetConfig().CHILD_SIZE_RANGE.Get() * cur_size),
                             MAX_CREATURE_SIZE - cur_size);
  if (Allocate_Main(ctx, alloc_size)) {
    GetRegister(dst) = cur_size;
    return true;
  } else return false;
}


bool cHardwareGX::Inst_SpawnDeme(cAvidaContext& ctx)
{
  organism->SpawnDeme();
  return true;
}

bool cHardwareGX::Inst_Kazi(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_AX);
  double percentProb = ((double) (GetRegister(reg_used) % 100)) / 100.0;
  if ( ctx.GetRandom().P(percentProb) ) organism->Kaboom(0);
  return true;
}

bool cHardwareGX::Inst_Kazi5(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_AX);
  double percentProb = ((double) (GetRegister(reg_used) % 100)) / 100.0;
  if ( ctx.GetRandom().P(percentProb) ) organism->Kaboom(5);
  return true;
}

bool cHardwareGX::Inst_Die(cAvidaContext& ctx)
{
  organism->Die();
  return true; 
}

// The inject instruction can be used instead of a divide command, paired
// with an allocate.  Note that for an inject to work, one needs to have a
// broad range for sizes allowed to be allocated.
//
// This command will cut out from read-head to write-head.
// It will then look at the template that follows the command and inject it
// into the complement template found in a neighboring organism.

bool cHardwareGX::Inst_Inject(cAvidaContext& ctx)
{
  AdjustHeads();
  const int start_pos = GetHead(nHardware::HEAD_READ).GetPosition();
  const int end_pos = GetHead(nHardware::HEAD_WRITE).GetPosition();
  const int inject_size = end_pos - start_pos;
  
  // Make sure the creature will still be above the minimum size,
  if (inject_size <= 0) {
    organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_ERROR, "inject: no code to inject");
    return false; // (inject fails)
  }
  if (start_pos < MIN_CREATURE_SIZE) {
    organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_ERROR, "inject: new size too small");
    return false; // (inject fails)
  }
  
  // Since its legal to cut out the injected piece, do so.
  cGenome inject_code( cGenomeUtil::Crop(GetMemory(), start_pos, end_pos) );
  GetMemory().Remove(start_pos, inject_size);
  
  // If we don't have a host, stop here.
  cOrganism * host_organism = organism->GetNeighbor();
  if (host_organism == NULL) return false;
  
  // Scan for the label to match...
  ReadLabel();
  
  // If there is no label, abort.
  if (GetLabel().GetSize() == 0) {
    organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_ERROR, "inject: label required");
    return false; // (inject fails)
  }
  
  // Search for the label in the host...
  GetLabel().Rotate(1, NUM_NOPS);
  
  const bool inject_signal = host_organism->GetHardware().InjectHost(GetLabel(), inject_code);
  if (inject_signal) {
    organism->Fault(FAULT_LOC_INJECT, FAULT_TYPE_WARNING, "inject: host too large.");
    return false; // Inject failed.
  }
  
  // Set the relevent flags.
  organism->GetPhenotype().IsModifier() = true;
  
  return inject_signal;
}


bool cHardwareGX::Inst_InjectRand(cAvidaContext& ctx)
{
  // Rotate to a random facing and then run the normal inject instruction
  const int num_neighbors = organism->GetNeighborhoodSize();
  organism->Rotate(ctx.GetRandom().GetUInt(num_neighbors));
  Inst_Inject(ctx);
  return true;
}


bool cHardwareGX::Inst_TaskGet(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  const int value = organism->GetNextInput();
  GetRegister(reg_used) = value;
  organism->DoInput(value);
  return true;
}


// @JEB - this instruction does more than two "gets" together
// it also (1) resets the inputs and (2) resets an organisms task counts
bool cHardwareGX::Inst_TaskGet2(cAvidaContext& ctx)
{
  // Randomize the inputs so they can't save numbers
  organism->GetOrgInterface().ResetInputs(ctx);   // Now re-randomize the inputs this organism sees
  organism->ClearInput();                         // Also clear their input buffers, or they can still claim
                                                  // rewards for numbers no longer in their environment!

  const int reg_used_1 = FindModifiedRegister(REG_BX);
  const int reg_used_2 = FindNextRegister(reg_used_1);
  
  const int value1 = organism->GetNextInput();
  GetRegister(reg_used_1) = value1;
  organism->DoInput(value1);
  
  const int value2 = organism->GetNextInput();
  GetRegister(reg_used_2) = value2;
  organism->DoInput(value2);
  
  // Clear the task number
  organism->GetPhenotype().ClearEffTaskCount();
  
  return true;
}

bool cHardwareGX::Inst_TaskStackGet(cAvidaContext& ctx)
{
  const int value = organism->GetNextInput();
  StackPush(value);
  organism->DoInput(value);
  return true;
}

bool cHardwareGX::Inst_TaskStackLoad(cAvidaContext& ctx)
{
  // @DMB - TODO: this should look at the input_size...
  for (int i = 0; i < 3; i++) 
    StackPush( organism->GetNextInput() );
  return true;
}

bool cHardwareGX::Inst_TaskPut(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int value = GetRegister(reg_used);
  GetRegister(reg_used) = 0;
  organism->DoOutput(ctx, value);
  return true;
}

bool cHardwareGX::Inst_TaskPutResetInputs(cAvidaContext& ctx)
{
  bool return_value = Inst_TaskPut(ctx);          // Do a normal put
  organism->GetOrgInterface().ResetInputs(ctx);   // Now re-randomize the inputs this organism sees
  organism->ClearInput();                         // Also clear their input buffers, or they can still claim
                                                  // rewards for numbers no longer in their environment!
  return return_value;
}

bool cHardwareGX::Inst_TaskIO(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  
  // Do the "put" component
  const int value_out = GetRegister(reg_used);
  organism->DoOutput(ctx, value_out);  // Check for tasks completed.
  
  // Do the "get" component
  const int value_in = organism->GetNextInput();
  GetRegister(reg_used) = value_in;
  organism->DoInput(value_in);
  return true;
}

bool cHardwareGX::Inst_TaskIO_Feedback(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);

  //check cur_bonus before the output
  double preOutputBonus = organism->GetPhenotype().GetCurBonus();
  
  // Do the "put" component
  const int value_out = GetRegister(reg_used);
  organism->DoOutput(ctx, value_out);  // Check for tasks completed.

  //check cur_merit after the output
  double postOutputBonus = organism->GetPhenotype().GetCurBonus(); 
  
  
  //push the effect of the IO on merit (+,0,-) to the active stack

  if (preOutputBonus > postOutputBonus){
    StackPush(-1);
    }
  else if (preOutputBonus == postOutputBonus){
    StackPush(0);
    }
  else if (preOutputBonus < postOutputBonus){
    StackPush(1);
    }
  else {
    assert(0);
    //Bollocks. There was an error.
    }


  


  
  // Do the "get" component
  const int value_in = organism->GetNextInput();
  GetRegister(reg_used) = value_in;
  organism->DoInput(value_in);
  return true;
}

bool cHardwareGX::Inst_MatchStrings(cAvidaContext& ctx)
{
	if (m_executedmatchstrings)
		return false;
	organism->DoOutput(ctx, 357913941);
	m_executedmatchstrings = true;
	return true;
}

bool cHardwareGX::Inst_Sell(cAvidaContext& ctx)
{
	int search_label = GetLabel().AsInt(3) % MARKET_SIZE;
	int send_value = GetRegister(REG_BX);
	int sell_price = m_world->GetConfig().SELL_PRICE.Get();
	organism->SellValue(send_value, search_label, sell_price);
	return true;
}

bool cHardwareGX::Inst_Buy(cAvidaContext& ctx)
{
	int search_label = GetLabel().AsInt(3) % MARKET_SIZE;
	int buy_price = m_world->GetConfig().BUY_PRICE.Get();
	GetRegister(REG_BX) = organism->BuyValue(search_label, buy_price);
	return true;
}

bool cHardwareGX::Inst_Send(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  organism->SendValue(GetRegister(reg_used));
  GetRegister(reg_used) = 0;
  return true;
}

bool cHardwareGX::Inst_Receive(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  GetRegister(reg_used) = organism->ReceiveValue();
  return true;
}

bool cHardwareGX::Inst_SenseLog2(cAvidaContext& ctx)
{
  return DoSense(ctx, 0, 2);
}

bool cHardwareGX::Inst_SenseUnit(cAvidaContext& ctx)
{
  return DoSense(ctx, 1, 1);
}

bool cHardwareGX::Inst_SenseMult100(cAvidaContext& ctx)
{
  return DoSense(ctx, 1, 100);
}

bool cHardwareGX::DoSense(cAvidaContext& ctx, int conversion_method, double base)
{
  // Returns the log2 amount of a resource or resources 
  // specified by modifying NOPs into register BX
  const tArray<double> & res_count = organism->GetOrgInterface().GetResources();

  // Arbitrarily set to BX since the conditionals use this directly.
  int reg_to_set = REG_BX;

  // There are no resources, return
  if (res_count.GetSize() == 0) return false;

  // Only recalculate logs if these values have changed
  static int last_num_resources = 0;
  static int max_label_length = 0;
  int num_nops = GetInstSet().GetNumNops();
  
  if ((last_num_resources != res_count.GetSize()))
  {
      max_label_length = (int) ceil(log((double)res_count.GetSize())/log((double)num_nops));
      last_num_resources = res_count.GetSize();
  }

  // Convert modifying NOPs to the index of the resource.
  // If there are fewer than the number of NOPs required
  // to uniquely specify a resource, then add together
  // a subset of resources (motivation: regulation can evolve
  // to be more specific if there is an advantage)
   
  // Find the maximum number of NOPs needed to specify this number of resources
  // Note: It's a bit wasteful to recalculate this every time and organisms will
  // definitely be confused if the number of resources changes during a run
  // because their mapping to resources will be disrupted
  
  // Attempt to read a label with this maximum length
  cHardwareGX::ReadLabel(max_label_length);
  
  // Find the length of the label that we actually obtained (max is max_reg_needed)
  int real_label_length = GetLabel().GetSize();
  
  // Start and end labels to define the start and end indices of  
  // resources that we need to add together
  cCodeLabel start_label = cCodeLabel(GetLabel());
  cCodeLabel   end_label = cCodeLabel(GetLabel());
  
  for (int i = 0; i < max_label_length - real_label_length; i++)
  {
    start_label.AddNop(0);
    end_label.AddNop(num_nops-1);
  }
  
  int start_index = start_label.AsInt(num_nops);
  int   end_index =   end_label.AsInt(num_nops);

  // If the label refers to ONLY resources that 
  // do not exist, then the operation fails
  if (start_index >= res_count.GetSize()) return false;

  // Otherwise sum all valid resources that it might refer to
  // (this will only be ONE if the label was of the maximum length).
  int resource_result = 0;
  for (int i = start_index; i <= end_index; i++)
  {
    // if it's a valid resource
    if (i < res_count.GetSize())
    {
      if (conversion_method == 0) // Log2
      {
        // (alternately you could assign min_int for zero resources, but
        // that would cause wierdness when adding sense values together)
        if (res_count[i] > 0) resource_result += (int)(log(res_count[i])/log(base));
      }
      else if (conversion_method == 1) // Addition of multiplied resource amount
      {
        int add_amount = (int) (res_count[i] * base);
        // Do some range checking to make sure we don't overflow
        resource_result = (INT_MAX - resource_result <= add_amount) ? INT_MAX : resource_result + add_amount;
      }
    } 
  }
    
  //Dump this value into an arbitrary register: BX
  GetRegister(reg_to_set) = resource_result;
  
  //We have to convert this to a different index that includes all degenerate labels possible: shortest to longest
  int sensed_index = 0;
  int on = 1;
  for (int i = 0; i < real_label_length; i++)
  {
    sensed_index += on;
    on *= num_nops;
  }
  sensed_index+= GetLabel().AsInt(num_nops);
  organism->GetPhenotype().IncSenseCount(sensed_index);
  
  return true; 

  // Note that we are converting <double> resources to <int> register values
}

void cHardwareGX::DoDonate(cOrganism* to_org)
{
  assert(to_org != NULL);

  const double merit_given = m_world->GetConfig().MERIT_GIVEN.Get();
  const double merit_received = m_world->GetConfig().MERIT_RECEIVED.Get();

  double cur_merit = organism->GetPhenotype().GetMerit().GetDouble();
  cur_merit -= merit_given;
  if(cur_merit < 0) cur_merit=0; 

  // Plug the current merit back into this organism and notify the scheduler.
  organism->UpdateMerit(cur_merit);
  
  // Update the merit of the organism being donated to...
  double other_merit = to_org->GetPhenotype().GetMerit().GetDouble();
  other_merit += merit_received;
  to_org->UpdateMerit(other_merit);
}

bool cHardwareGX::Inst_DonateRandom(cAvidaContext& ctx)
{
  
  if (organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }

  organism->GetPhenotype().IncDonates();
  organism->GetPhenotype().SetIsDonorRand();

  // Turn to a random neighbor, get it, and turn back...
  int neighbor_id = ctx.GetRandom().GetInt(organism->GetNeighborhoodSize());
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(1);
  cOrganism * neighbor = organism->GetNeighbor();
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(-1);
  
  // Donate only if we have found a neighbor.
  if (neighbor != NULL) {
    DoDonate(neighbor);
    
    //print out how often random donations go to kin
    /*
    static ofstream kinDistanceFile("kinDistance.dat");
    kinDistanceFile << (genotype->GetPhyloDistance(neighbor->GetGenotype())<=1) << " ";
    kinDistanceFile << (genotype->GetPhyloDistance(neighbor->GetGenotype())<=2) << " ";
    kinDistanceFile << (genotype->GetPhyloDistance(neighbor->GetGenotype())<=3) << " ";
    kinDistanceFile << genotype->GetPhyloDistance(neighbor->GetGenotype());
    kinDistanceFile << endl; 
    */
    neighbor->GetPhenotype().SetIsReceiverRand();
  }

  return true;
}


bool cHardwareGX::Inst_DonateKin(cAvidaContext& ctx)
{
  if (organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }
  
  organism->GetPhenotype().IncDonates();
  organism->GetPhenotype().SetIsDonorKin();


  // Find the target as the first Kin found in the neighborhood.
  const int num_neighbors = organism->GetNeighborhoodSize();
  
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(1);
  cOrganism * neighbor = organism->GetNeighbor();
  
  // If there is no max distance, just take the random neighbor we're facing.
  const int max_dist = m_world->GetConfig().MAX_DONATE_KIN_DIST.Get();
  if (max_dist != -1) {
    int max_id = neighbor_id + num_neighbors;
    bool found = false;
    cGenotype* genotype = organism->GetGenotype();
    while (neighbor_id < max_id) {
      neighbor = organism->GetNeighbor();
      if (neighbor != NULL &&
          genotype->GetPhyloDistance(neighbor->GetGenotype()) <= max_dist) {
        found = true;
        break;
      }
      organism->Rotate(1);
      neighbor_id++;
    }
    if (found == false) neighbor = NULL;
  }
  
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(-1);
  
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL){
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverKin();
  }
  return true;
}

bool cHardwareGX::Inst_DonateEditDist(cAvidaContext& ctx)
{
  if (organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }

  organism->GetPhenotype().IncDonates();
  organism->GetPhenotype().SetIsDonorEdit();
  
  // Find the target as the first Kin found in the neighborhood.
  const int num_neighbors = organism->GetNeighborhoodSize();
  
  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(1);
  cOrganism* neighbor = organism->GetNeighbor();
  
  // If there is no max edit distance, take the random neighbor we're facing.
  const int max_dist = m_world->GetConfig().MAX_DONATE_EDIT_DIST.Get();
  if (max_dist != -1) {
    int max_id = neighbor_id + num_neighbors;
    bool found = false;
    while (neighbor_id < max_id) {
      neighbor = organism->GetNeighbor();
      int edit_dist = max_dist + 1;
      if (neighbor != NULL) {
        edit_dist = cGenomeUtil::FindEditDistance(organism->GetGenome(),
                                                  neighbor->GetGenome());
      }
      if (edit_dist <= max_dist) {
        found = true;
        break;
      }
      organism->Rotate(1);
      neighbor_id++;
    }
    if (found == false) neighbor = NULL;
  }
  
  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(-1);
  
  // Donate only if we have found a close enough relative...
  if (neighbor != NULL){
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverEdit();
  }
  return true;
}

bool cHardwareGX::Inst_DonateGreenBeardGene(cAvidaContext& ctx)
{
  //this donates to organisms that have this instruction anywhere
  //in their genome (see Dawkins 1976, The Selfish Gene, for 
  //the history of the theory and the name 'green beard'
  cPhenotype & phenotype = organism->GetPhenotype();

  if (organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }

  phenotype.IncDonates();
  phenotype.SetIsDonorGbg();

  // Find the target as the first match found in the neighborhood.

  //get the neighborhood size
  const int num_neighbors = organism->GetNeighborhoodSize();

  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(1);
  cOrganism * neighbor = organism->GetNeighbor();

  int max_id = neighbor_id + num_neighbors;
 
  //we have not found a match yet
  bool found = false;

  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
      neighbor = organism->GetNeighbor();

      //if neighbor exists, do they have the green beard gene?
      if (neighbor != NULL) {
          const cGenome & neighbor_genome = neighbor->GetGenome();

          // for each instruction in the genome...
          for(int i=0;i<neighbor_genome.GetSize();i++){

            // ...see if it is donate-gbg
            if (neighbor_genome[i] == IP().GetInst()) {
              found = true;
              break;
            }
	    
          }
      }
      
      // stop searching through the neighbors if we already found one
      if (found == true);{
    	break;
      }
  
      organism->Rotate(1);
      neighbor_id++;
  }

    if (found == false) neighbor = NULL;

  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(-1);

  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverGbg();
  }
  
  return true;
  
}

bool cHardwareGX::Inst_DonateTrueGreenBeard(cAvidaContext& ctx)
{
  //this donates to organisms that have this instruction anywhere
  //in their genome AND their parents excuted it
  //(see Dawkins 1976, The Selfish Gene, for 
  //the history of the theory and the name 'green beard'
  //  cout << "i am about to donate to a green beard" << endl;
  cPhenotype & phenotype = organism->GetPhenotype();

  if (organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }

  phenotype.IncDonates();
  phenotype.SetIsDonorTrueGb();

  // Find the target as the first match found in the neighborhood.

  //get the neighborhood size
  const int num_neighbors = organism->GetNeighborhoodSize();

  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(1);
  cOrganism * neighbor = organism->GetNeighbor();

  int max_id = neighbor_id + num_neighbors;
 
  //we have not found a match yet
  bool found = false;

  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
      neighbor = organism->GetNeighbor();
      //if neighbor exists, AND if their parent attempted to donate,
      if (neighbor != NULL && neighbor->GetPhenotype().IsDonorTrueGbLast()) {
          const cGenome & neighbor_genome = neighbor->GetGenome();

          // for each instruction in the genome...
          for(int i=0;i<neighbor_genome.GetSize();i++){

            // ...see if it is donate-tgb, if so, we found a target
            if (neighbor_genome[i] == IP().GetInst()) {
              found = true;
              break;
            }
	    
          }
      }
      
      // stop searching through the neighbors if we already found one
      if (found == true);{
    	break;
      }
  
      organism->Rotate(1);
      neighbor_id++;
  }

    if (found == false) neighbor = NULL;

  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(-1);

  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverTrueGb();
  }

  
  return true;
  
}

bool cHardwareGX::Inst_DonateThreshGreenBeard(cAvidaContext& ctx)
{
  //this donates to organisms that have this instruction anywhere
  //in their genome AND their parents excuted it >=THRESHOLD number of times
  //(see Dawkins 1976, The Selfish Gene, for 
  //the history of the theory and the name 'green beard'
  //  cout << "i am about to donate to a green beard" << endl;
  cPhenotype & phenotype = organism->GetPhenotype();

  if (organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }


  phenotype.IncDonates();
  phenotype.SetIsDonorThreshGb();
  phenotype.IncNumThreshGbDonations();

  // Find the target as the first match found in the neighborhood.

  //get the neighborhood size
  const int num_neighbors = organism->GetNeighborhoodSize();

  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(1);
  cOrganism * neighbor = organism->GetNeighbor();

  int max_id = neighbor_id + num_neighbors;
 
  //we have not found a match yet
  bool found = false;

  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
      neighbor = organism->GetNeighbor();
      //if neighbor exists, AND if their parent attempted to donate >= threshhold,
      if (neighbor != NULL && neighbor->GetPhenotype().GetNumThreshGbDonationsLast()>= m_world->GetConfig().MIN_GB_DONATE_THRESHOLD.Get() ) {
          const cGenome & neighbor_genome = neighbor->GetGenome();

          // for each instruction in the genome...
          for(int i=0;i<neighbor_genome.GetSize();i++){

	         // ...see if it is donate-threshgb, if so, we found a target
            if (neighbor_genome[i] == IP().GetInst()) {
              found = true;
              break;
            }
	    
          }
      }
      
      // stop searching through the neighbors if we already found one
      if (found == true);{
    	break;
      }
  
      organism->Rotate(1);
      neighbor_id++;
  }

    if (found == false) neighbor = NULL;

  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(-1);

  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverThreshGb();
    // cout << "************ neighbor->GetPhenotype().GetNumThreshGbDonationsLast() is " << neighbor->GetPhenotype().GetNumThreshGbDonationsLast() << endl;
    
  }

  return true;
  
}


bool cHardwareGX::Inst_DonateQuantaThreshGreenBeard(cAvidaContext& ctx)
{
  // this donates to organisms that have this instruction anywhere
  // in their genome AND their parents excuted it more than a
  // THRESHOLD number of times where that threshold depend on the
  // number of times the individual's parents attempted to donate
  // using this instruction.  The threshold levels are multiples of
  // the quanta value set in genesis, and the highest level that
  // the donor qualifies for is the one used.

  // (see Dawkins 1976, The Selfish Gene, for 
  // the history of the theory and the name 'green beard'
  //  cout << "i am about to donate to a green beard" << endl;
  cPhenotype & phenotype = organism->GetPhenotype();

  if (phenotype.GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }

  phenotype.IncDonates();
  phenotype.SetIsDonorQuantaThreshGb();
  phenotype.IncNumQuantaThreshGbDonations();
  //cout << endl << "quanta_threshgb attempt.. " ;


  // Find the target as the first match found in the neighborhood.

  //get the neighborhood size
  const int num_neighbors = organism->GetNeighborhoodSize();

  // Turn to face a random neighbor
  int neighbor_id = ctx.GetRandom().GetInt(num_neighbors);
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(1);
  cOrganism * neighbor = organism->GetNeighbor();

  int max_id = neighbor_id + num_neighbors;
 
  //we have not found a match yet
  bool found = false;

  // Get the quanta (step size) between threshold levels.
  const int donate_quanta = m_world->GetConfig().DONATE_THRESH_QUANTA.Get();
  
  // Calculate what quanta level we should be at for this individual.  We do a
  // math trick to make sure its the next lowest event multiple of donate_quanta.
  const int quanta_donate_thresh =
    (phenotype.GetNumQuantaThreshGbDonationsLast() / donate_quanta) * donate_quanta;
  //cout << " phenotype.GetNumQuantaThreshGbDonationsLast() is " << phenotype.GetNumQuantaThreshGbDonationsLast();
  //cout << " quanta thresh=  " << quanta_donate_thresh;
  // rotate through orgs in neighborhood  
  while (neighbor_id < max_id) {
      neighbor = organism->GetNeighbor();
      //if neighbor exists, AND if their parent attempted to donate >= threshhold,
      if (neighbor != NULL &&
	  neighbor->GetPhenotype().GetNumQuantaThreshGbDonationsLast() >= quanta_donate_thresh) {

          const cGenome & neighbor_genome = neighbor->GetGenome();

          // for each instruction in the genome...
          for(int i=0;i<neighbor_genome.GetSize();i++){

	         // ...see if it is donate-quantagb, if so, we found a target
            if (neighbor_genome[i] == IP().GetInst()) {
              found = true;
              break;
            }
	    
          }
      }
      
      // stop searching through the neighbors if we already found one
      if (found == true);{
    	break;
      }
  
      organism->Rotate(1);
      neighbor_id++;
  }

    if (found == false) neighbor = NULL;

  // Put the facing back where it was.
  for (int i = 0; i < neighbor_id; i++) organism->Rotate(-1);

  // Donate only if we have found a close enough relative...
  if (neighbor != NULL) {
    DoDonate(neighbor);
    neighbor->GetPhenotype().SetIsReceiverQuantaThreshGb();
    //cout << " ************ neighbor->GetPhenotype().GetNumQuantaThreshGbDonationsLast() is " << neighbor->GetPhenotype().GetNumQuantaThreshGbDonationsLast();
    
  }

  return true;
  
}


bool cHardwareGX::Inst_DonateNULL(cAvidaContext& ctx)
{
  if (organism->GetPhenotype().GetCurNumDonates() > m_world->GetConfig().MAX_DONATES.Get()) {
    return false;
  }

  organism->GetPhenotype().IncDonates();
  organism->GetPhenotype().SetIsDonorNull();
  
  // This is a fake donate command that causes the organism to lose merit,
  // but no one else to gain any.
  
  const double merit_given = m_world->GetConfig().MERIT_GIVEN.Get();
  double cur_merit = organism->GetPhenotype().GetMerit().GetDouble();
  cur_merit -= merit_given;
  
  // Plug the current merit back into this organism and notify the scheduler.
  organism->UpdateMerit(cur_merit);
  
  return true;
}


bool cHardwareGX::Inst_SearchF(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  const int search_size = FindLabel(1).GetPosition() - IP().GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  return true;
}

bool cHardwareGX::Inst_SearchB(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  const int search_size = IP().GetPosition() - FindLabel(-1).GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  return true;
}

bool cHardwareGX::Inst_MemSize(cAvidaContext& ctx)
{
  GetRegister(FindModifiedRegister(REG_BX)) = GetMemory().GetSize();
  return true;
}


bool cHardwareGX::Inst_RotateL(cAvidaContext& ctx)
{
  const int num_neighbors = organism->GetNeighborhoodSize();
  
  // If this organism has no neighbors, ignore rotate.
  if (num_neighbors == 0) return false;
  
  ReadLabel();
  
  // Always rotate at least once.
  organism->Rotate(-1);
  
  // If there is no label, then the one rotation was all we want.
  if (!GetLabel().GetSize()) return true;
  
  // Rotate until a complement label is found (or all have been checked).
  GetLabel().Rotate(1, NUM_NOPS);
  for (int i = 1; i < num_neighbors; i++) {
    cOrganism* neighbor = organism->GetNeighbor();
    
    if (neighbor != NULL && neighbor->GetHardware().FindLabelFull(GetLabel()).InMemory()) return true;
    
    // Otherwise keep rotating...
    organism->Rotate(-1);
  }
  return true;
}

bool cHardwareGX::Inst_RotateR(cAvidaContext& ctx)
{
  const int num_neighbors = organism->GetNeighborhoodSize();
  
  // If this organism has no neighbors, ignore rotate.
  if (num_neighbors == 0) return false;
  
  ReadLabel();
  
  // Always rotate at least once.
  organism->Rotate(1);
  
  // If there is no label, then the one rotation was all we want.
  if (!GetLabel().GetSize()) return true;
  
  // Rotate until a complement label is found (or all have been checked).
  GetLabel().Rotate(1, NUM_NOPS);
  for (int i = 1; i < num_neighbors; i++) {
    cOrganism* neighbor = organism->GetNeighbor();
    
    if (neighbor != NULL && neighbor->GetHardware().FindLabelFull(GetLabel()).InMemory()) return true;
    
    // Otherwise keep rotating...
    organism->Rotate(1);
  }
  return true;
}

bool cHardwareGX::Inst_SetCopyMut(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const int new_mut_rate = Max(GetRegister(reg_used), 1 );
  organism->SetCopyMutProb(static_cast<double>(new_mut_rate) / 10000.0);
  return true;
}

bool cHardwareGX::Inst_ModCopyMut(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_BX);
  const double new_mut_rate = organism->GetCopyMutProb() + static_cast<double>(GetRegister(reg_used)) / 10000.0;
  if (new_mut_rate > 0.0) organism->SetCopyMutProb(new_mut_rate);
  return true;
}

// Energy use

bool cHardwareGX::Inst_ZeroEnergyUsed(cAvidaContext& ctx)
{
  // Typically, this instruction should be triggered by a REACTION
  organism->GetPhenotype().SetTimeUsed(0); 
  return true;  
}


// Head-based instructions

//bool cHardwareGX::Inst_SetHead(cAvidaContext& ctx)
//{
//  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
//  m_threads[m_cur_thread].cur_head = static_cast<unsigned char>(head_used);
//  return true;
//}

bool cHardwareGX::Inst_AdvanceHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_WRITE);
  GetHead(head_used).Advance();
  return true;
}

bool cHardwareGX::Inst_MoveHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  const int target = nHardware::HEAD_FLOW;
  GetHead(head_used).Set(GetHead(target));
  if (head_used == nHardware::HEAD_IP) m_advance_ip = false;
  return true;
}

bool cHardwareGX::Inst_JumpHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  GetHead(head_used).Jump(GetRegister(REG_CX) );
  // JEB - probably shouldn't advance inst ptr after jumping here?
  // Any negative number jumps to the beginning of the genome (pos 0)
  // and then we immediately advance past that first instruction.
  return true;
}

bool cHardwareGX::Inst_GetHead(cAvidaContext& ctx)
{
  const int head_used = FindModifiedHead(nHardware::HEAD_IP);
  GetRegister(REG_CX) = GetHead(head_used).GetPosition();
  return true;
}

bool cHardwareGX::Inst_IfLabel(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  if (GetLabel() != GetReadLabel())  IP().Advance();
  return true;
}

// This is a variation on IfLabel that will skip the next command if the "if"
// is false, but it will also skip all nops following that command.
bool cHardwareGX::Inst_IfLabel2(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  if (GetLabel() != GetReadLabel()) {
    IP().Advance();
    if (m_inst_set->IsNop( IP().GetNextInst() ))  IP().Advance();
  }
  return true;
}


bool cHardwareGX::Inst_HeadDivide(cAvidaContext& ctx)
{
  return Divide_Main(ctx);
}


bool cHardwareGX::Inst_HeadRead(cAvidaContext& ctx)
{
  const int dst = REG_BX;
  
  const int head_id = FindModifiedHead(nHardware::HEAD_READ);
  GetHead(head_id).Adjust();
  sCPUStats & cpu_stats = organism->CPUStats();
  
  // Mutations only occur on the read, for the moment.
  int read_inst = 0;
  if (organism->TestCopyMut(ctx)) {
    read_inst = m_inst_set->GetRandomInst(ctx).GetOp();
    cpu_stats.mut_stats.copy_mut_count++;  // @CAO, hope this is good!
  } else {
    read_inst = GetHead(head_id).GetInst().GetOp();
  }
  GetRegister(dst) = read_inst;
  ReadInst(read_inst);
  
  cpu_stats.mut_stats.copies_exec++;  // @CAO, this too..
  GetHead(head_id).Advance();
  return true;
}

bool cHardwareGX::Inst_HeadWrite(cAvidaContext& ctx)
{
  const int src = REG_BX;
  const int head_id = FindModifiedHead(nHardware::HEAD_WRITE);
  cHeadCPU& active_head = GetHead(head_id);
  
  active_head.Adjust();
  
  int value = GetRegister(src);
  if (value < 0 || value >= m_inst_set->GetSize()) value = 0;
  
  active_head.SetInst(cInstruction(value));
  active_head.SetFlagCopied();
  
  // Advance the head after write...
  active_head++;
  return true;
}

bool cHardwareGX::Inst_HeadCopy(cAvidaContext& ctx)
{
  // For the moment, this cannot be nop-modified.
  cHeadCPU& read_head = GetHead(nHardware::HEAD_READ);
  cHeadCPU& write_head = GetHead(nHardware::HEAD_WRITE);
  sCPUStats& cpu_stats = organism->CPUStats();
  
  read_head.Adjust();
  write_head.Adjust();
  
  // Do mutations.
  cInstruction read_inst = read_head.GetInst();
  ReadInst(read_inst.GetOp());
  if (organism->TestCopyMut(ctx)) {
    read_inst = m_inst_set->GetRandomInst(ctx);
    cpu_stats.mut_stats.copy_mut_count++; 
    write_head.SetFlagMutated();
    write_head.SetFlagCopyMut();
  }
  
  cpu_stats.mut_stats.copies_exec++;
  
  write_head.SetInst(read_inst);
  write_head.SetFlagCopied();  // Set the copied flag...
  
  read_head.Advance();
  write_head.Advance();
  return true;
}

bool cHardwareGX::HeadCopy_ErrorCorrect(cAvidaContext& ctx, double reduction)
{
  // For the moment, this cannot be nop-modified.
  cHeadCPU & read_head = GetHead(nHardware::HEAD_READ);
  cHeadCPU & write_head = GetHead(nHardware::HEAD_WRITE);
  sCPUStats & cpu_stats = organism->CPUStats();
  
  read_head.Adjust();
  write_head.Adjust();
  
  // Do mutations.
  cInstruction read_inst = read_head.GetInst();
  ReadInst(read_inst.GetOp());
  if ( ctx.GetRandom().P(organism->GetCopyMutProb() / reduction) ) {
    read_inst = m_inst_set->GetRandomInst(ctx);
    cpu_stats.mut_stats.copy_mut_count++; 
    write_head.SetFlagMutated();
    write_head.SetFlagCopyMut();
    //organism->GetPhenotype().IsMutated() = true;
  }
  
  cpu_stats.mut_stats.copies_exec++;
  
  write_head.SetInst(read_inst);
  write_head.SetFlagCopied();  // Set the copied flag...
  
  read_head.Advance();
  write_head.Advance();
  return true;
}

bool cHardwareGX::Inst_HeadCopy2(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 2); }
bool cHardwareGX::Inst_HeadCopy3(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 3); }
bool cHardwareGX::Inst_HeadCopy4(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 4); }
bool cHardwareGX::Inst_HeadCopy5(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 5); }
bool cHardwareGX::Inst_HeadCopy6(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 6); }
bool cHardwareGX::Inst_HeadCopy7(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 7); }
bool cHardwareGX::Inst_HeadCopy8(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 8); }
bool cHardwareGX::Inst_HeadCopy9(cAvidaContext& ctx)  { return HeadCopy_ErrorCorrect(ctx, 9); }
bool cHardwareGX::Inst_HeadCopy10(cAvidaContext& ctx) { return HeadCopy_ErrorCorrect(ctx, 10); }

bool cHardwareGX::Inst_HeadSearch(cAvidaContext& ctx)
{
  ReadLabel();
  GetLabel().Rotate(1, NUM_NOPS);
  cHeadCPU found_pos = FindLabel(0);
  const int search_size = found_pos.GetPosition() - IP().GetPosition();
  GetRegister(REG_BX) = search_size;
  GetRegister(REG_CX) = GetLabel().GetSize();
  GetHead(nHardware::HEAD_FLOW).Set(found_pos);
  GetHead(nHardware::HEAD_FLOW).Advance();
  return true; 
}

bool cHardwareGX::Inst_SetFlow(cAvidaContext& ctx)
{
  const int reg_used = FindModifiedRegister(REG_CX);
  GetHead(nHardware::HEAD_FLOW).Set(GetRegister(reg_used));
return true; 
}

//// Placebo insts ////
bool cHardwareGX::Inst_Skip(cAvidaContext& ctx)
{
  IP().Advance();
  return true;
}


/*! This instruction allocates a new programid with a zero length genome
and moves the write head of the current programid to it.
*/
bool cHardwareGX::Inst_NewProgramid(cAvidaContext& ctx, bool executable, bool bindable, bool readable)
{
  // Do some maintenance on the programid where the write head was previously.
  // (1) Adjust the number of heads on it; this could make it executable!
  // (2) \todo Delete if it has no instructions or is below a certain size?
  int write_head_contacted = GetHead(nHardware::HEAD_WRITE).GetMemSpace();
  m_programids[write_head_contacted]->RemoveContactingHead(GetHead(nHardware::HEAD_WRITE));
  GetHead(nHardware::HEAD_WRITE).Set(0, m_current->m_id); // Immediately set the write head back to itself
  
  // If we've reached the programid limit, then deal with that
  if(m_programids.size() >= MAX_PROGRAMIDS) {
    //Decide on a programid to destroy, currently highest number of cpu cycles executed
    //\todo more methods of choosing..
  
    if (PROGRAMID_REPLACEMENT_METHOD == 0) {
      // Don't replace (they can still die of old age!)
      return false;
    } else if(PROGRAMID_REPLACEMENT_METHOD == 1) {
      // Replace oldest programid
      int destroy_index = -1;
      int max_cpu_cycles_used = -1;
      for (unsigned int i=0; i<m_programids.size(); i++) {
        if (m_programids[i]->GetCPUCyclesUsed() > max_cpu_cycles_used) {
          max_cpu_cycles_used = m_programids[i]->GetCPUCyclesUsed();
          destroy_index = i;
        }
      }
      assert(destroy_index>=0);
      RemoveProgramid(destroy_index);
      
      // Also replace anything that its WRITE head contacted (unless itself)...
      // to prevent accumulating partially copied programids
      if(write_head_contacted != destroy_index) {
        RemoveProgramid(write_head_contacted);
      }
    }
  }
  
  // Create the new programid and add it to the list
  cGenome new_genome(1);
  programid_ptr new_programid = new cProgramid(new_genome, this);
  new_programid->m_executable = executable;
  new_programid->m_bindable = bindable;
  new_programid->m_readable = readable;
  AddProgramid(new_programid);
  
  // Set the write head to the newly allocated programid
  new_programid->AddContactingHead(GetHead(nHardware::HEAD_WRITE));
  GetHead(nHardware::HEAD_WRITE).Set(0, new_programid->GetID());
  
  return true;
}

bool cHardwareGX::Inst_Site(cAvidaContext& ctx)
{
  // Do nothing except move past the label
  ReadLabel();
  return true;
}

/*! This instruction reads the trailing label and tries to match it against other 
programids in the CPU.  Note: labels that follow a match instruction in the 
programid to be matched against are *ignored*.  Note that match should be a
'free' instruction, that is, it should not cost anything to execute.

\todo Determine if Inst_Match should wait for a match, as opposed to letting the
cProgramid continue.  This seems like a good idea...

\todo Have to determine the correct placement of the heads on Bind.  If we're not
careful, we'll allow two RNAPs to bind to each other and start copying each other's
genome.  Probably a bad idea. @JEB Well, at least those selfish solutions would die.
Temp Soln @JEB -- added a flag to programids so that only certain programids are bindable
*/
bool cHardwareGX::Inst_Bind(cAvidaContext& ctx) 
{
  // Get the label that we're trying to match.
  // Do this first to advance IP past it.
  ReadLabel();

  // Binding fails if we are already bound!
  cHeadProgramid& read = GetHead(nHardware::HEAD_READ);
  if(read.GetMemSpace() != m_current->GetID()) return false;
  cHeadProgramid& write = GetHead(nHardware::HEAD_WRITE);

  // Now, select another programid to match against.
  // Go through all *other* programids looking for matches 
  std::vector<cMatchSite> all_matches;
  for(programid_list::iterator i=m_programids.begin(); i!=m_programids.end(); ++i) {
    // Don't bind to ourself, or to whatever programid our write head is attached to.
    if((*i != m_current) && ((*i)->GetID() != write.GetMemSpace())) {
      std::vector<cMatchSite> matches = (*i)->Sites(GetLabel());
      all_matches.insert(all_matches.end(), matches.begin(), matches.end());
    }
  }  
  
  // The instruction failed if there were no matches
  if(all_matches.size() == 0) return false;
  
  // Otherwise set the read head to a random match
  unsigned int c = ctx.GetRandom().GetUInt(all_matches.size());
  
  // Ok, it matched.  Bind the current programid's read head to the matched site.
  m_current->Bind(nHardware::HEAD_READ, all_matches[c]);

  // And we're done.
  return true;
}


/*! This instruction attempts to locate two programids that have the same site.
If two such programids are found, BX is set to 2, otherwise BX is set to 0.

This instruction is well-suited for finding two genomes.  For example, the following
instruction sequence may be used to locate two genomes with an origin of replication,
and if found, trigger a cell division:
bind2
nop-B
nop-C
if-not-0
p-divide
*/
bool cHardwareGX::Inst_Bind2(cAvidaContext& ctx)
{
  // Get the label we're searching for.
  ReadLabel();
  
  // Search for matches to this label.
  std::vector<cMatchSite> bindable;
  for(programid_list::iterator i=m_programids.begin(); i!=m_programids.end(); ++i) {
    if(*i != m_current) {
      std::vector<cMatchSite> matches = (*i)->Sites(GetLabel());
      // Now, we only want one match from each programid; we'll take a random one.
      if(matches.size()>0) {
        bindable.push_back(matches[ctx.GetRandom().GetInt(matches.size())]);
      }
    }
  }
  
  // Select two of the matches at random.
  if(bindable.size()>=2) {
    int first = ctx.GetRandom().GetInt(bindable.size());
    int second = ctx.GetRandom().GetInt(bindable.size());
    while(first == second) { second = ctx.GetRandom().GetUInt(bindable.size()); }
    assert(bindable[first].m_programid->GetID() != bindable[second].m_programid->GetID());
    assert(bindable[first].m_programid->GetBindable());
    assert(bindable[second].m_programid->GetBindable());
    assert(bindable[first].m_programid->GetReadable());
    assert(bindable[second].m_programid->GetReadable());

    // If the caller is already bound to other programids, detach.
    m_current->Detach();
    
    // And attach this programid's read and write heads to the indexed organisms.
    // It *is* possible that the caller could do "bad things" now.
    m_current->Bind(nHardware::HEAD_READ, bindable[first]);
    m_current->Bind(nHardware::HEAD_WRITE, bindable[second]);

    // Finally, set BX to indicate that bind2 worked, and return.
    GetRegister(REG_BX) = 2;
    return true;
  }
  
  // Bind2 didn't work.
  GetRegister(REG_BX) = 0;
  return false;
}


bool cHardwareGX::Inst_IfBind(cAvidaContext& ctx) 
{
  // Normal Bind
  bool ret = Inst_Bind(ctx);
  
  //Skip the next instruction if binding was not successful
  if (!ret) IP().Advance();
  
  return ret;
}


bool cHardwareGX::Inst_IfBind2(cAvidaContext& ctx)
{
  // Normal Bind2
  bool ret = Inst_Bind2(ctx);
  
  //Skip the next instruction if binding was not successful
  if (!ret) IP().Advance();
  
  return ret;
}


/*! This instruction puts the total number of binding sites found in BX 
Currently it is used to keep track of whether genome division has completed.*/
bool cHardwareGX::Inst_NumSites(cAvidaContext& ctx)
{
  // Get the label that we're trying to match.
  ReadLabel();

  // Go through all *other* programids counting matches 
  int num_sites = 0;
  for(programid_list::iterator i=m_programids.begin(); i!=m_programids.end(); ++i) {
    if (*i != m_current) {
      num_sites += (*i)->Sites(GetLabel()).size();
    }
  }  
  
  GetRegister(REG_BX) = num_sites;
  return true;
}


/*! This instruction is like h-copy, except:
(1) It does nothing if the read and write head are not on OTHER programids
(2) It dissociates the read head if it encounters the complement of the label that was used in the
    match instruction that put the read head on its current target
*/
bool cHardwareGX::Inst_ProgramidCopy(cAvidaContext& ctx)
{
  cHeadProgramid& write = GetHead(nHardware::HEAD_WRITE);
  cHeadProgramid& read = GetHead(nHardware::HEAD_READ);
  read.Adjust(); // Strange things can happen (like we're reading from a programid that was being written).
  
  // Don't copy if this programid's write or read head is on itself
  if(read.GetMemSpace() == m_current->GetID()) return false;
  if(write.GetMemSpace() == m_current->GetID()) return false;
  
  // Don't copy if the source is not readable
  if(!m_programids[read.GetMemSpace()]->GetReadable()) return false;

  // If a copy is mutated in after a bind2, then the read head could be at the 
  // end of a genome.  The copy fails, and we should probably break the bind, too.
  if(read.GetPosition() >= read.GetMemory().GetSize()) {
    m_current->Detach();
    return false;
  }
  
  // Keep track of whether the last non-NOP we copied was a site
  if(GetInstSet().IsNop(read.GetInst())) {
    m_current->m_copying_label.AddNop(GetInstSet().GetNopMod(read.GetInst()));
  } else {
    m_current->m_copying_site = (read.GetInst() == GetInstSet().GetInst("site"));
    m_current->m_copying_label.Clear();
  }
  
  // Allocate space for one additional instruction if the write head is at the end
  // Otherwise the write head will improperly 
  if(write.GetMemory().GetSize() == write.GetPosition() + 1) {
    write.GetMemory().Resize(write.GetMemory().GetSize() + 1);
  }
  
  // \todo The timing of deletion, change, insertion mutation checks matters
  // I'm not sure this is right @JEB
    
  // Normal h-copy, unless a deletion occured
  if (!organism->TestCopyDel(ctx)) {
      // Normal h-copy
    bool ret = Inst_HeadCopy(ctx);
  }
  
  // Divide Insertion
  if (organism->TestCopyIns(ctx)) {
    write.GetMemory().Insert(write.GetPosition(), GetInstSet().GetRandomInst(ctx));
    // Advance the write head;
    write++;
  }
  
  // Peek at the next inst to see if it is a NOP
  // If it isn't, then we can compare the label and possibly fall off  
  if(m_current->m_copying_site) {
    if((!GetInstSet().IsNop(read.GetInst()) && (m_current->m_terminator_label == m_current->m_copying_label))) {
      // Move read head off of old target and back to itself
      read.GetProgramid()->RemoveContactingHead(read);
      read.Set(0, m_current->m_id);
      
      //Shrink the programid we were on by one inst
      if (write.GetMemory().GetSize()>1)
      {
        write.GetMemory().Resize( write.GetMemory().GetSize() - 1 );
      }
      
      // \to do, we would apply insertion/deletion on divide instructions here
      // if we want them to happen for each new programid that is produced
      //  // Handle Divide Mutations...
      
      // This would be equivalent to
      // Divide_DoMutations(ctx, mut_multiplier);
      // But that operates on m_child_genome, currently
      
      return true;
    }
  }
   
  return true;
}


/*! This instruction creates a new organism by randomly dividing the programids with
the daughter cell. Currently we convert the daughter genomes back into one list with pseudo-instructions
telling us where new programids start and some of their properties. This is pretty inefficient.
*/
bool cHardwareGX::Inst_ProgramidDivide(cAvidaContext& ctx)
{
  //This stuff is usually set by Divide_CheckViable, leaving it zero causes problems
  cPhenotype& phenotype = organism->GetPhenotype();
  organism->GetPhenotype().SetLinesExecuted(1);
  organism->GetPhenotype().SetLinesCopied(1);
  
  // Let's make sure that things seem sane.
  cHeadProgramid& read = GetHead(nHardware::HEAD_READ); // The parent.
  cHeadProgramid& write = GetHead(nHardware::HEAD_WRITE); // The offspring.
  
  // If either of these heads are on m_current, this instruction fails.
  if(read.GetMemSpace() == m_current->GetID()) return false;
  if(write.GetMemSpace() == m_current->GetID()) return false;

  // It should never be the case that the read and write heads are on the same programid.
  assert(read.GetMemSpace() != write.GetMemSpace());
  // Actually, it can happen with bind2 @JEB
  
  // If the read and write heads are on the same programid, then fail
  if (read.GetMemSpace() == write.GetMemSpace()) return false;
  
  // If we're not bound to two bindable programids, this instruction fails.
  if(!m_programids[read.GetMemSpace()]->GetBindable()) return false;
  if(!m_programids[write.GetMemSpace()]->GetBindable()) return false;
  
  // Now, let's keep track of two different lists of programids, one for the parent,
  // and one for the offspring.
  programid_list parent;
  programid_list offspring;
  // We're also going to do all our work on a temporary list, so that we can fail
  // without affecting the state of the caller.
  programid_list all(m_programids);
  // This is a list of fragments, to be deleted once we've passed the viability check.
  programid_list fragments;

  // The currently executing programid called the divide instruction.  The caller
  // is (hopefully) a DNA polymerase, therefore it knows which genome fragments go where.
  parent.push_back(m_programids[read.GetMemSpace()]); // The parent's genome.
  all[read.GetMemSpace()] = 0;
  offspring.push_back(m_programids[write.GetMemSpace()]); // The offspring's genome.
  //offspring.back()->SetBindable(true);
  all[write.GetMemSpace()] = 0;
  
  // Locate and remove all incomplete genomes, identified by programids that have
  // write heads on them.
  for(programid_list::iterator i=all.begin(); i!=all.end(); ++i) {
    // Does this programid currently have it's write head somewhere?
    if((*i != 0) && ((*i)->GetHead(nHardware::HEAD_WRITE).GetMemSpace() != (*i)->GetID())) {
      // Yes - It is likely an incomplete genome fragment, so don't
      // allow it to propagate.
      fragments.push_back(all[(*i)->GetHead(nHardware::HEAD_WRITE).GetMemSpace()]);
      all[(*i)->GetHead(nHardware::HEAD_WRITE).GetMemSpace()] = 0;
    }
  }
  
  // Divvy up the programids.
  for(programid_list::iterator i=all.begin(); i!=all.end(); ++i) {
    if(*i != 0) {
      if(ctx.GetRandom().GetUInt(2) == 0) {
        // Offspring!
        offspring.push_back(*i);
      } else {
        // Parent!
        parent.push_back(*i);
      }
    }
  }
    
  ///// Failure conditions (custom divide_check_viable)
  // It is possible that the divide kills the child and the parent
  // Each must have genomic programids of some minimum length
  // and an executable programid (otherwise it is inviable)
  // For now we leave a zombie mother to die of old age
  // but do not permit creating an inviable daughter

  // Conditions for successful replication
  int num_daughter_programids = 0;
  int daughter_genome_length = 0;
  bool daughter_has_executable = false;
  bool daughter_has_bindable = false;
  
  // Calculate these conditions.
  for(programid_list::iterator i=offspring.begin(); i!=offspring.end(); ++i) {
    ++num_daughter_programids;
    if((*i)->GetReadable()) {
      daughter_genome_length += (*i)->GetMemory().GetSize();
    }
    daughter_has_executable = daughter_has_executable || (*i)->GetExecutable();
    daughter_has_bindable = daughter_has_bindable || (*i)->GetBindable();
  }
  assert(daughter_has_bindable); // We know this should be there...
  
  // And check them.  Note that if this check fails, we have *not* modified the
  // state of the calling programid.
  if((num_daughter_programids == 0) 
     || (!daughter_has_executable) 
     || (daughter_genome_length < 50)) { 
    // \todo link to original genome length
    return false;
  }
  
  // Ok, we're good to go.  We have to create the offspring's genome and delete the
  // offspring's programids from m_programids.
  cCPUMemory& child_genome = organism->ChildGenome();
  child_genome.Resize(1);
  if (m_world->GetVerbosity() >= VERBOSE_DETAILS) std::cout << "-=OFFSPRING=-" << endl;
  for(programid_list::iterator i=offspring.begin(); i!=offspring.end(); ++i) {
    (*i)->AppendLinearGenome(child_genome);
    if (m_world->GetVerbosity() >= VERBOSE_DETAILS) (*i)->PrintGenome(std::cout);
    delete *i;
    *i = 0;
  }
  
  // Now clean up the parent.
  m_programids.clear();
  if (m_world->GetVerbosity() >= VERBOSE_DETAILS) std::cout << "-=PARENT=-" << endl;
  for(programid_list::iterator i=parent.begin(); i!=parent.end(); ++i) {
    AddProgramid(*i);
    if (m_world->GetVerbosity() >= VERBOSE_DETAILS) (*i)->PrintGenome(std::cout);
  }  
  
  // And delete the fragments.
  for(programid_list::iterator i=fragments.begin(); i!=fragments.end(); ++i) {
    delete *i;
  }
    
  // Activate the child
  bool parent_alive = organism->ActivateDivide(ctx);

  // Mother viability checks could go here.  
  m_just_divided = true;
  return true;
}


//! Adds a new programid to the current cHardwareGX.
void cHardwareGX::AddProgramid(programid_ptr programid) 
{ 
  programid->m_id = m_programids.size();
  programid->ResetHeads();
//  programid->ResetCPUCyclesUsed();
  programid->m_contacting_heads = 0;
  m_programids.push_back(programid);   
}


void cHardwareGX::RemoveProgramid(unsigned int remove_index) 
{
  assert(remove_index<m_programids.size());
  
  programid_ptr save=m_current;  
  m_current = m_programids[remove_index];
  unsigned int write_head_contacted = (unsigned int)GetHead(nHardware::HEAD_WRITE).GetMemSpace();
  
  // First update the contacting head count for any cProgramids the heads 
  // of the programid to be removed might have been on
  m_current->Detach();
//  m_programids[GetHead(nHardware::HEAD_READ).GetMemSpace()]->RemoveContactingHead(GetHead(nHardware::HEAD_READ));
//  m_programids[GetHead(nHardware::HEAD_WRITE).GetMemSpace()]->RemoveContactingHead(GetHead(nHardware::HEAD_WRITE));

  // Update the programid list
  delete *(m_programids.begin()+remove_index);
  m_programids.erase(m_programids.begin()+remove_index);
  // Add adjust all the programid ids from the removed programid to the end.
  for(programid_list::iterator i=m_programids.begin()+remove_index; i!=m_programids.end(); ++i) {
    --(*i)->m_id;
  }

  // We also need to make sure heads are on the correct memory
  // spaces, since that indexing changes with the programid list
  for(unsigned int i=0; i< m_programids.size(); i++) {
    programid_ptr p = m_programids[i];
    for(int j=0; j<NUM_HEADS; j++) {
      // We removed the thing they were writing from or reading to
      // For now, just put the write head back on themselves (inactivating further copies)
      // Might want to also reset the read head..
      if (p->m_heads[j].GetMemSpace() == (int)remove_index) {
        p->m_heads[j].Set(0, p->m_id);
      } else if (p->m_heads[j].GetMemSpace() > (int)remove_index) {
        p->m_heads[j].Set(p->m_heads[j].GetPosition(), p->m_heads[j].GetMemSpace() - 1);
      }
    }
  }
  
  // Finally, also delete whatever programid our write head contacted (if not ourself!)
  if(write_head_contacted != remove_index) {
    RemoveProgramid( (write_head_contacted > remove_index) ? write_head_contacted - 1 : write_head_contacted);
  }

  m_current = save;
}


/*! Construct this cProgramid, and initialize hardware resources.
*/
cHardwareGX::cProgramid::cProgramid(const cGenome& genome, cHardwareGX* hardware)
: m_memory(genome)
, m_executable(false)
, m_bindable(false)
, m_readable(false)
, m_copying_site(false)
, m_cpu_cycles_used(0)
, m_gx_hardware(hardware)
, m_unique_id(hardware->m_last_unique_id_assigned++)
{
  assert(m_gx_hardware!=0);
  for(int i=0; i<NUM_HEADS; ++i) {
    m_heads[i].Reset(hardware);
  }

  // Check what flags should be set on this programid.
  for(int i=0; i<m_memory.GetSize();) {
    if(m_memory[i]==GetInst("PROGRAMID")) { 
      m_memory.Remove(i);
      continue;
    }
    if(m_memory[i]==GetInst("EXECUTABLE")) { 
      m_memory.Remove(i); 
      m_executable=true;
      continue;
    }
    if(m_memory[i]==GetInst("BINDABLE")) { 
      m_memory.Remove(i);
      m_bindable=true;
      continue;
    }
    if(m_memory[i]==GetInst("READABLE")) { 
      m_memory.Remove(i);
      m_readable=true;
      continue;
    }
    ++i;
  }
}


/*! Append this programid's genome to the passed in genome.  Include the tags
that specify what this programid is capable of.
*/
void cHardwareGX::cProgramid::AppendLinearGenome(cCPUMemory& genome) {
  genome.Append(GetInst("PROGRAMID"));
  if(GetExecutable()) { genome.Append(GetInst("EXECUTABLE")); }
  if(GetBindable()) { genome.Append(GetInst("BINDABLE")); }
  if(GetReadable()) { genome.Append(GetInst("READABLE")); }
  genome.Append(m_memory);
}


void cHardwareGX::cProgramid::PrintGenome(std::ostream& out) {
  out << "Programid ID: " << m_id << " UID:" << m_unique_id << endl;
  if(GetExecutable()) out << " EXECUTABLE";
  if(GetBindable()) out << " BINDABLE";
  if(GetReadable()) out << " READABLE";
  out << endl;
  out << " Mem (" << GetMemory().GetSize() << "):" << " " << GetMemory().AsString() << endl;
  out.flush();
}


/*! This method attempts to match this cProgramid with the passed-in label.  If the
match is succesful, it returns true and a cMatchSite object that may be used to bind
to the programid.  If the match is unsuccessful, it return false and the cMatchSite
object points to null.

A "successful" match is one where this cProgramid has a series of NOPs that are similar
to the the passed-in label.  A number of configuration options (will eventually)
control how precisely the NOPs must be related (e.g., exact, all-but-one, etc.).
*/
std::vector<cHardwareGX::cMatchSite> cHardwareGX::cProgramid::Sites(const cCodeLabel& label) 
{
  std::vector<cHardwareGX::cMatchSite> matches;
  if(!m_bindable) return matches;
  
  cInstruction site_inst = m_gx_hardware->GetInstSet().GetInst("site");
  
  // Create a new search head at the beginning of our memory space
  // \to do doesn't properly find wrap-around matches overlapping the origin of the memory
  
  //Find the first non-NOP and start there (this allows ups to wrap around correctly)
  cHeadCPU search_head(m_gx_hardware, 0, m_id);
  int first_non_nop = -1;
  do {
    if ( !m_gx_hardware->m_inst_set->IsNop(search_head.GetInst()) )
    {
      first_non_nop = search_head.GetPosition();
      break;
    }
    search_head++;
  } while (search_head.GetPosition() != 0);
  
  // This genome is all NOPs...
  if (first_non_nop == -1) return matches;
  
  //keep track of the first time we find a non-NOP instruction (finish when we reach it a second time)
  int site_pos = -1;
  cCodeLabel site_label;
  
  // Start at this instruction
  search_head.Set(first_non_nop, m_id);
  do {
  
    if (search_head.GetInst() == site_inst)
    {
      site_pos = search_head.GetPosition();
      site_label.Clear();
    }
    else if ( m_gx_hardware->m_inst_set->IsNop(search_head.GetInst()) && (site_pos != -1) )
    {
      // Add NOPs to the current label
      site_label.AddNop( m_gx_hardware->m_inst_set->GetNopMod( search_head.GetInst() ) );
    }
    else // Any other non-NOP instruction means to stop looking for terminator matches
    {
      site_pos = -1;
    }
     
    // Is the next inst a NOP?
    // If not, then check our current label for termination
    if (site_pos != -1)
    {
      int old_pos = search_head.GetPosition();
      search_head++;
      if ( !m_gx_hardware->m_inst_set->IsNop( search_head.GetInst()) )
      {
        if ( site_label == label )
        {
          cMatchSite match;
          match.m_programid = this;
          match.m_site = site_pos; // We return is exactly on the site
          match.m_label = site_label;
          matches.push_back(match);
        }
      }
      
      // Grrr. Heads don't wrap backwards properly!~! Can't just search_head--; No idea why.
      search_head.Set(old_pos, m_id);
    }
    
    search_head++;
  } while ( search_head.GetPosition() != first_non_nop ); // back at the beginning 

}


/*! Bind attaches parts of this cProgramid to the cProgramid specified in the
passed-in cMatchSite.  Currently, we only support binding the read head to a
location in the genome of the other cProgramid, but this will be extended later.
*/
void cHardwareGX::cProgramid::Bind(nHardware::tHeads head, cMatchSite& site) {
  // We set the terminator site label to the complement of the one that was bound.
  m_terminator_label = site.m_label;
  m_terminator_label.Rotate(1, NUM_NOPS);
  
  // Set the head.
  if(GetHead(head).GetMemSpace() != GetID()) {
    // Head is somewhere else; remove it
    m_gx_hardware->m_programids[GetHead(head).GetMemSpace()]->RemoveContactingHead(GetHead(head));
  }
  
  // Now attach it to the passed-in match site
  m_gx_hardware->m_programids[site.m_programid->GetID()]->AddContactingHead(GetHead(head));
  GetHead(head).Set(site.m_site, site.m_programid->GetID());
}


/*! This method detaches the caller's heads from programids that it is connected
to.  It also updates the head contact counts.
*/
void cHardwareGX::cProgramid::Detach() {
  int head = GetHead(nHardware::HEAD_WRITE).GetMemSpace();
  m_gx_hardware->m_programids[head]->RemoveContactingHead(GetHead(nHardware::HEAD_WRITE));
  GetHead(nHardware::HEAD_WRITE).Set(0, GetID());
  
  head = GetHead(nHardware::HEAD_READ).GetMemSpace();
  m_gx_hardware->m_programids[head]->RemoveContactingHead(GetHead(nHardware::HEAD_READ));
  GetHead(nHardware::HEAD_READ).Set(0, GetID());
}
