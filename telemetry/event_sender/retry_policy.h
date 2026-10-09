#pragma once
// 2: transient, keep event and retry later. 3: operator intervention required.
inline int sendFailureExit(bool started, bool guarded, int curlExit, int http) {
    if(!started)return 3;
    if(guarded)return 2;
    if(curlExit==58||curlExit==60||curlExit==77||curlExit==90||curlExit==91)return 3;
    if(curlExit!=0)return 2;
    if(http>=400&&http<500&&http!=408&&http!=429)return 3;
    // Redirects and successful HTTP with an invalid ACK are protocol/config failures.
    if(http>=200&&http<400)return 3;
    return 2;
}
