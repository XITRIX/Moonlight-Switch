#include <cassert>
#include "../research-backend/kaihen/common/research_codefs.h"
int main() {
    ResearchCodeFsOpen in={1,2,{},UINT64_C(0x040077201be50000)};
    assert(researchCodeFsValid(&in));
    in.program_id=UINT64_C(0x01004d300c5ae000); in.attributes=0;
    assert(researchCodeFsValid(&in));
    for (unsigned storage=0;storage<7;storage++) {
        in.storage=storage; assert(bool(researchCodeFsValid(&in))==(storage==2));
    }
    in.storage=2; in.attributes=2; assert(!researchCodeFsValid(&in));
    in.attributes=1; in.program_id=UINT64_C(0x040000000000004a);
    assert(!researchCodeFsValid(&in));
    in.program_id=UINT64_C(0x040077201be50000);
    for (unsigned i=0;i<6;i++) { in.reserved[i]=1; assert(!researchCodeFsValid(&in)); in.reserved[i]=0; }
}
