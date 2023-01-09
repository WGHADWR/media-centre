//
// Created by cyy on 2022/11/22.
//

#ifndef VIDEOPLAYER_AV_PACKET_QUEUE_H
#define VIDEOPLAYER_AV_PACKET_QUEUE_H

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "SDL2/SDL.h"
};

class AVPacketQueueItem {
public:
    int id;
    AVPacket *packet;
    AVPacketQueueItem *next;

    AVPacketQueueItem();
    ~AVPacketQueueItem();
};

class AVPacketQueue {

private:
    SDL_mutex *lock;
    AVPacketQueueItem *first = nullptr;
    AVPacketQueueItem *last = nullptr;

public:
    int serial = 0;
    int len;

    AVPacketQueue();
    void put(AVPacket *pkt);
    AVPacketQueueItem* take();

    ~AVPacketQueue();
};

#endif //VIDEOPLAYER_AV_PACKET_QUEUE_H
