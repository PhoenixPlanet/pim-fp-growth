#ifndef COMMON_H
#define COMMON_H

struct FPArrayEntry {
    uint32_t item;
    int32_t parent_pos;
    uint32_t support;
    uint32_t depth;
};

struct ElePosEntry {
    uint32_t item;
    uint32_t pos;
    uint32_t support;
    uint32_t candidate_start_idx;
};

struct CandidateEntry {
    uint32_t prefix_item;
    uint32_t suffix_item;
    uint32_t suffix_item_pos;
    uint32_t support;
};

#endif // COMMON_H