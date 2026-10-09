#include "retry_policy.h"
#include <cstdio>
int main() {
    if(sendFailureExit(true,false,7,0)!=2 || sendFailureExit(true,false,28,0)!=2 ||
       sendFailureExit(true,false,0,503)!=2 || sendFailureExit(true,false,0,429)!=2 ||
       sendFailureExit(true,false,0,408)!=2 || sendFailureExit(true,true,0,200)!=2 ||
       sendFailureExit(true,false,60,0)!=3 || sendFailureExit(true,false,58,0)!=3 ||
       sendFailureExit(true,false,0,403)!=3 || sendFailureExit(true,false,0,409)!=3 ||
       sendFailureExit(true,false,0,200)!=3 || sendFailureExit(true,false,0,302)!=3 ||
       sendFailureExit(false,false,-1,0)!=3)return 1;
    std::puts("PASS: transient/configuration/protocol/TLS retry classification");
}
