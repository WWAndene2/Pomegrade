// The azahar_check harnesses' stand-in for Azahar's common/archives.h: the checks never save a state, so
// classes are not registered with Boost serialization (whose library the harnesses don't link: its
// registration ran at start-up and crashed on the missing symbols).
#pragma once
#define SERIALIZE_EXPORT_IMPL(A)
