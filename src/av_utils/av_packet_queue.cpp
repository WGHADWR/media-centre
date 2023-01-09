//
// Created by cyy on 2022/11/22.
//

#include "av_packet_queue.h"

AVPacketQueueItem::AVPacketQueueItem() {
    this->id = -1;
    this->packet = nullptr;
    this->next = nullptr;
}

AVPacketQueue::AVPacketQueue() {
    this->lock = SDL_CreateMutex();
    this->first = nullptr;
    this->last = nullptr;
    this->len = 0;
}

void AVPacketQueue::put(AVPacket *pkt) {
    SDL_LockMutex(this->lock);

    auto *item = new AVPacketQueueItem;
    // memset(item, 0, sizeof(AVPacketQueueItem));
    AVPacket *dst = av_packet_alloc();
    if (!dst) {
        SDL_Log("av_packet_alloc error");
        return;
    }

//    av_packet_ref(dst, pkt);
    av_packet_move_ref(dst, pkt);
    item->packet = dst;
    item->id = this->serial++;

    if (this->first == nullptr) {
        this->first = item;
        this->last = this->first;
    } else {
        this->last->next = item;
        this->last = item;
    }
    this->len += 1;
    SDL_UnlockMutex(this->lock);
}

AVPacketQueueItem* AVPacketQueue::take() {
    SDL_LockMutex(this->lock);
    if (this->first == nullptr) {
        SDL_UnlockMutex(this->lock);
        return nullptr;
    }
    AVPacketQueueItem *item = this->first;
    this->first = item->next;
    this->len -= 1;
    SDL_UnlockMutex(this->lock);
    item->next = nullptr;
    return item;
}

AVPacketQueue::~AVPacketQueue() {
    SDL_DestroyMutex(this->lock);
    delete(this->first);
    delete(this->last);
}

AVPacketQueueItem::~AVPacketQueueItem() {
    av_packet_unref(this->packet);
    delete(this->next);
}