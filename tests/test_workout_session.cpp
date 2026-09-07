#include "exercise/WorkoutSession.h"
#include <iostream>
int main() {
    int failures=0;
    auto check=[&](bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';if(!ok)++failures;};
    wakeai::WorkoutSession s;
    check(!s.finish(),"cannot finish before begin");
    s.begin(3);
    check(!s.updateCount(-2)&&s.count()==0,"negative input ignored");
    check(!s.updateCount(2)&&!s.finish(),"below target cannot finish");
    check(!s.updateCount(1)&&s.count()==2,"out-of-order count cannot regress");
    check(s.updateCount(3)&&s.reached(),"target transition fires once");
    check(!s.updateCount(3)&&!s.updateCount(4)&&s.count()==3,"freeze on target");
    check(s.finish()&&!s.finish(),"finish idempotent");
    check(!s.updateCount(10),"ignore late worker update");
    s.begin(2);check(s.count()==0&&!s.reached()&&!s.finished(),"new session clears state");
    s.cancel();check(!s.updateCount(2)&&!s.finish(),"cancel does not succeed");
    s.begin(0);check(s.target()==1&&s.updateCount(1)&&s.finish(),"positive target enforced");
    return failures?1:0;
}
