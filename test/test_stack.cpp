/*
   Copyright (C) 2018 - 2023 by Huan Nguyen in Secure Systems
   Lab, Stony Brook University, Stony Brook, NY 11794.
*/

#include "../utility.h"
#include "../state.h"
#include "../domain.h"
#include "../framework.h"
#include "../program.h"
#include "../function.h"
#include "../scc.h"
#include "../block.h"
#include "../insn.h"
#include "../rtl.h"
#include "../expr.h"
#include "../../../run/config.h"

using namespace std;
using namespace SBA;
/* -------------------------------------------------------------------------- */
function<void(const UnitId&, AbsVal&)> init_sym = [](const UnitId& id,
AbsVal& out) -> void {
   /* BaseLH */
   ABSVAL(BaseLH,out) = !id.bounded()? BaseLH(BaseLH::T::TOP): BaseLH(get_sym(id));
};

function<void(IMM c, AbsVal&)> init_const = [](IMM c, AbsVal& out) -> void {
   std::get<BaseLH::ID>(out.value) = BaseLH(Range(c,c));
   std::get<BaseStride::ID>(out.value) = BaseStride(BaseStride::Base(c));
};

State::StateConfig config{true, true, true, false, &init_sym, &init_const};
/* -------------------------------------------------------------------------- */
Program* p = nullptr;
Function* f = nullptr;
IMM i_total = 0;
unordered_set<IMM> checked_func;
fstream f_jt;
/* -------------------------------------------------------------------------- */
vector<IMM> read_entries() {
   string s;
   vector<IMM> entries;
   vector<IMM> filtered;
   unordered_set<IMM> offsets;

   fstream fmeta("/tmp/obj.func", fstream::in);
   while (getline(fmeta, s))
      entries.push_back(Util::to_int(s));
   fmeta.close();
   fstream fmeta2("/tmp/obj.offset", fstream::in);
   while (getline(fmeta2, s))
      offsets.insert(Util::to_int("0x"+s));
   fmeta2.close();

   IMM prev_entry = -100;
   sort(entries.begin(), entries.end());
   if (entries.size() > 10000) {
      for (auto entry: entries)
         if (offsets.contains(entry)) {
            if (entry - prev_entry < 20 && !filtered.empty())
               filtered.pop_back();
            filtered.push_back(entry);
            prev_entry = entry;
         }
   }
   else {
      for (auto entry: entries)
         if (offsets.contains(entry))
            filtered.push_back(entry);
   }
   LOG2("#entries = " << entries.size());
   LOG2("#filtered_entries = " << filtered.size());
   return filtered;
}
/* -------------------------------------------------------------------------- */
void func_stats(IMM& f_cnt, IMM& s_cnt, IMM& b_cnt, IMM& i_cnt) {
   IMM b_cnt2 = 0;
   IMM i_cnt2 = 0;
   ++f_cnt;
   s_cnt += f->scc_list().size();
   for (auto scc: f->scc_list()) {
      b_cnt += scc->block_list().size();
      b_cnt2 += scc->block_list().size();
      for (auto b: scc->block_list()) {
         i_cnt += b->insn_list().size();
         i_cnt2 += b->insn_list().size();
      }
   }
   LOG2("function " << f->offset() << ": b_count = " << b_cnt2
                                   << "; i_count = " << i_cnt2);
}
/* -------------------------------------------------------------------------- */
bool should_analyse() {
   if (checked_func.contains(f->offset()))
      return false;
   for (auto scc: f->scc_list())
   for (auto b: scc->block_list())
   for (auto i: b->insn_list())
   if (i->jump() && i->indirect() && !p->indirect_targets().contains(i->offset()))
      return true;
   checked_func.insert(f->offset());
   return false;
}
/* -------------------------------------------------------------------------- */
int main(int argc, char **argv) {
   LOG_START("/tmp/sba.log");
   Framework::config(TOOL_PATH"auto/output_old_ocaml.auto");
   f_jt.open("/tmp/obj.jtable", fstream::out);

   auto entries = read_entries();
   if (entries.empty())
      return 0;
   p = Framework::create_program("/tmp/obj", entries);

   if (p != nullptr) {
      p->read_binary("/tmp/obj");
      IMM f_cnt = 0, s_cnt = 0, b_cnt = 0, i_cnt = 0;
      for (auto entry: p->entries()) {
         f = p->func(entry);
         if (f != nullptr) {
            if (should_analyse()) {
               func_stats(f_cnt, s_cnt, b_cnt, i_cnt);
               f->analyse(config);
            }
            f->summary();
         }
      }
      LOG2("--> f_count = " << f_cnt);
      LOG2("--> s_count = " << s_cnt);
      LOG2("--> b_count = " << b_cnt);
      LOG2("--> i_count = " << i_cnt);
      LOG2("=====================================");
      LOG2("+++++++++++++++++++++++++++++++++++++");
      LOG2("=====================================");
      i_total += i_cnt;
   }
   f_jt.close();
   Framework::print_stats();
   LOG_STOP();
   return 0;
}
