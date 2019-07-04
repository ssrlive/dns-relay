// DNS Relay.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream>

using namespace std;
unique_ptr<DnsServer> server;
void signalHandler(int sig);
int debugLevel;
#define CHECKDEBUG() {debugLevel==0?true:false; }
#define CHECKFULLDEBUG() {debugLevel==2?true:false; }
// TODO: Reference additional headers your program requires here.
