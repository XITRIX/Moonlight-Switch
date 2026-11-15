// SPDX-License-Identifier: MIT
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Single asynchronous job at a time. HTTP/main thread owns these calls.
int agentSubmit(const void *packet, size_t size);
void agentPoll(void);
bool agentBusy(void);
const char *agentStatus(void);
uint64_t agentRevision(void);
void agentSummary(char *output, size_t capacity);
void agentLogTail(char *output, size_t capacity);
bool agentTakeNetworkReset(void);
void agentShutdown(void);
