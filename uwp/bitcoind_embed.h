// Copyright (c) 2026 Gianluca Mazza
// SPDX-License-Identifier: MIT
#pragma once

// Declares the embed entry provided by Bitcoin Core bitcoind.cpp when built
// with -DBITCOIND_EMBED (patch 0003). C++ linkage (not extern "C").
int BitcoindMain(int argc, char* argv[]);
