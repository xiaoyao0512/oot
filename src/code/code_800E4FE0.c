#include "global.h"

#define SAMPLES_TO_OVERPRODUCE 0x10
#define EXTRA_BUFFERED_AI_SAMPLES_TARGET 0x80

typedef enum {
    CHAN_UPD_UNK_0,           // 0
    CHAN_UPD_VOL_SCALE,       // 1
    CHAN_UPD_VOL,             // 2
    CHAN_UPD_PAN_SIGNED,      // 3
    CHAN_UPD_FREQ_SCALE,      // 4
    CHAN_UPD_REVERB,          // 5
    CHAN_UPD_SCRIPT_IO,       // 6
    CHAN_UPD_PAN_UNSIGNED,    // 7
    CHAN_UPD_STOP_SOMETHING2, // 8
    CHAN_UPD_MUTE_BEHAVE,     // 9
    CHAN_UPD_VIBE_X8,         // 10
    CHAN_UPD_VIBE_X32,        // 11
    CHAN_UPD_UNK_0F,          // 12
    CHAN_UPD_UNK_20,          // 13
    CHAN_UPD_STEREO           // 14
} ChannelUpdateType;

void func_800E6300(SequenceChannel* channel, AudioCmd* arg1);
void func_800E59AC(s32 playerIdx, s32 fadeTimer);
void Audio_InitMesgQueues(void);
AudioTask* func_800E5000(void);
void Audio_ProcessCmds(u32);
void func_800E6128(SequencePlayer* seqPlayer, AudioCmd* arg1);
void func_800E5958(s32 playerIdx, s32 fadeTimer);
s32 func_800E66C0(s32 arg0);

// AudioMgr_Retrace
AudioTask* func_800E4FE0(void) {
    return func_800E5000();
}

/**
 * This is Audio_Update for the audio thread
 */
AudioTask* func_800E5000(void) {
    static s32 sMaxAbiCmdCnt = 0x80;
    static AudioTask* sWaitingAudioTask = NULL;
    u32 samplesRemainingInAi;
    s32 abiCmdCnt;
    s32 pad;
    s32 j;
    s32 sp5C;
    s16* currAiBuffer;
    OSTask_t* task;
    s32 index;
    u32 sp4C;
    s32 sp48;
    s32 i;

    gAudioContext.totalTaskCount++;
    if (gAudioContext.totalTaskCount % (gAudioContext.audioBufferParameters.specUnk4) != 0) {
        if (D_801755D0 != NULL) {
            D_801755D0();
        }

        if ((gAudioContext.totalTaskCount % gAudioContext.audioBufferParameters.specUnk4) + 1 ==
            gAudioContext.audioBufferParameters.specUnk4) {
            return sWaitingAudioTask;
        } else {
            return NULL;
        }
    }

    osSendMesg(gAudioContext.taskStartQueueP, (OSMesg)gAudioContext.totalTaskCount, OS_MESG_NOBLOCK);
    gAudioContext.rspTaskIndex ^= 1;
    gAudioContext.curAiBufIndex++;
    gAudioContext.curAiBufIndex %= 3;
    index = (gAudioContext.curAiBufIndex - 2 + 3) % 3;
    samplesRemainingInAi = osAiGetLength() / 4;

    if (gAudioContext.resetTimer < 16) {
        if (gAudioContext.aiBufLengths[index] != 0) {
            osAiSetNextBuffer(gAudioContext.aiBuffers[index], gAudioContext.aiBufLengths[index] * 4);
            if (gAudioContext.aiBuffers[index]) {}
            if (gAudioContext.aiBufLengths[index]) {}
        }
    }

    if (D_801755D0 != NULL) {
        D_801755D0();
    }

    sp5C = gAudioContext.curAudioFrameDmaCount;
    for (i = 0; i < gAudioContext.curAudioFrameDmaCount; i++) {
        if (osRecvMesg(&gAudioContext.currAudioFrameDmaQueue, NULL, OS_MESG_NOBLOCK) == 0) {
            sp5C--;
        }
    }

    if (sp5C != 0) {
        for (i = 0; i < sp5C; i++) {
            osRecvMesg(&gAudioContext.currAudioFrameDmaQueue, NULL, OS_MESG_BLOCK);
        }
    }

    sp48 = MQ_GET_COUNT(&gAudioContext.currAudioFrameDmaQueue);
    if (sp48 != 0) {
        for (i = 0; i < sp48; i++) {
            osRecvMesg(&gAudioContext.currAudioFrameDmaQueue, NULL, OS_MESG_NOBLOCK);
        }
    }

    gAudioContext.curAudioFrameDmaCount = 0;
    AudioLoad_DecreaseSampleDmaTtls();
    AudioLoad_ProcessLoads(gAudioContext.resetStatus);
    AudioLoad_ProcessScriptLoads();

    if (gAudioContext.resetStatus != 0) {
        if (AudioHeap_ResetStep() == 0) {
            if (gAudioContext.resetStatus == 0) {
                osSendMesg(gAudioContext.audioResetQueueP, (OSMesg)(u32)gAudioContext.audioResetSpecIdToLoad,
                           OS_MESG_NOBLOCK);
            }

            sWaitingAudioTask = NULL;
            return NULL;
        }
    }

    if (gAudioContext.resetTimer > 16) {
        return NULL;
    }
    if (gAudioContext.resetTimer != 0) {
        gAudioContext.resetTimer++;
    }

    gAudioContext.curTask = &gAudioContext.rspTask[gAudioContext.rspTaskIndex];
    gAudioContext.curAbiCmdBuf = gAudioContext.abiCmdBufs[gAudioContext.rspTaskIndex];

    index = gAudioContext.curAiBufIndex;
    currAiBuffer = gAudioContext.aiBuffers[index];

    gAudioContext.aiBufLengths[index] =
        (s16)((((gAudioContext.audioBufferParameters.samplesPerFrameTarget - samplesRemainingInAi) +
                EXTRA_BUFFERED_AI_SAMPLES_TARGET) &
               ~0xF) +
              SAMPLES_TO_OVERPRODUCE);
    if (gAudioContext.aiBufLengths[index] < gAudioContext.audioBufferParameters.minAiBufferLength) {
        gAudioContext.aiBufLengths[index] = gAudioContext.audioBufferParameters.minAiBufferLength;
    }

    if (gAudioContext.aiBufLengths[index] > gAudioContext.audioBufferParameters.maxAiBufferLength) {
        gAudioContext.aiBufLengths[index] = gAudioContext.audioBufferParameters.maxAiBufferLength;
    }

    j = 0;
    if (gAudioContext.resetStatus == 0) {
        // msg = 0000RREE R = read pos, E = End Pos
        while (osRecvMesg(gAudioContext.cmdProcQueueP, (OSMesg*)&sp4C, OS_MESG_NOBLOCK) != -1) {
            if (1) {}
            if (1) {}
            if (1) {}
            Audio_ProcessCmds(sp4C);
            j++;
        }
        if ((j == 0) && (gAudioContext.cmdQueueFinished)) {
            Audio_ScheduleProcessCmds();
        }
    }

    gAudioContext.curAbiCmdBuf =
        AudioSynth_Update(gAudioContext.curAbiCmdBuf, &abiCmdCnt, currAiBuffer, gAudioContext.aiBufLengths[index]);
    gAudioContext.audioRandom = (gAudioContext.audioRandom + gAudioContext.totalTaskCount) * osGetCount();
    gAudioContext.audioRandom =
        gAudioContext.aiBuffers[index][gAudioContext.totalTaskCount & 0xFF] + gAudioContext.audioRandom;
    gWaveSamples[8] = (s16*)(((u8*)func_800E4FE0) + (gAudioContext.audioRandom & 0xFFF0));

    index = gAudioContext.rspTaskIndex;
    gAudioContext.curTask->msgQueue = NULL;
    gAudioContext.curTask->unk_44 = NULL;

    task = &gAudioContext.curTask->task.t;
    task->type = M_AUDTASK;
    task->flags = 0;
    task->ucode_boot = aspMainTextStart;
    task->ucode_boot_size = SP_UCODE_SIZE;
    task->ucode_data_size = (size_t)(aspMainDataEnd - aspMainDataStart) * sizeof(u64) - 1;
    task->ucode = aspMainTextStart;
    task->ucode_data = aspMainDataStart;
    task->ucode_size = SP_UCODE_SIZE;
    task->dram_stack = NULL;
    task->dram_stack_size = 0;
    task->output_buff = NULL;
    task->output_buff_size = NULL;
    if (1) {}
    task->data_ptr = (u64*)gAudioContext.abiCmdBufs[index];
    task->data_size = abiCmdCnt * sizeof(Acmd);
    task->yield_data_ptr = NULL;
    task->yield_data_size = 0;

    if (sMaxAbiCmdCnt < abiCmdCnt) {
        sMaxAbiCmdCnt = abiCmdCnt;
    }

    if (gAudioContext.audioBufferParameters.specUnk4 == 1) {
        return gAudioContext.curTask;
    } else {
        sWaitingAudioTask = gAudioContext.curTask;
        return NULL;
    }
}

#define ACMD_SND_MDE ((u32)0xF0000000)
#define ACMD_MUTE ((u32)0xF1000000)

void func_800E5584(AudioCmd* cmd) {
    s32 i;
    s32 pad;
    s32 pad2;
    u32 temp_a1_5;
    u32 temp_t7;

    switch (cmd->op) {
        case 0x81:
            AudioLoad_SyncLoadSeqParts(cmd->arg1, cmd->arg2);
            break;

        case 0x82:
            AudioLoad_SyncInitSeqPlayer(cmd->arg0, cmd->arg1, cmd->arg2);
            func_800E59AC(cmd->arg0, cmd->data);
            break;

        case 0x85:
            AudioLoad_SyncInitSeqPlayerSkipTicks(cmd->arg0, cmd->arg1, cmd->data);
            break;

        case 0x83:
            if (gAudioContext.seqPlayers[cmd->arg0].enabled) {
                if (cmd->asInt == 0) {
                    AudioSeq_SequencePlayerDisableAsFinished(&gAudioContext.seqPlayers[cmd->arg0]);
                } else {
                    func_800E5958(cmd->arg0, cmd->asInt);
                }
            }
            break;

        case 0xF0:
            gAudioContext.soundMode = cmd->asUInt;
            break;

        case 0xF1:
            for (i = 0; i < gAudioContext.audioBufferParameters.numSequencePlayers; i++) {
                SequencePlayer* seqPlayer = &gAudioContext.seqPlayers[i];

                seqPlayer->muted = 1;
                seqPlayer->recalculateVolume = 1;
            }
            break;

        case 0xF2:
            if (cmd->asUInt == 1) {
                for (i = 0; i < gAudioContext.numNotes; i++) {
                    Note* note = &gAudioContext.notes[i];
                    NoteSubEu* subEu = &note->noteSubEu;

                    if (subEu->bitField0.enabled && note->playbackState.unk_04 == 0) {
                        if (note->playbackState.parentLayer->channel->muteBehavior & 8) {
                            subEu->bitField0.finished = 1;
                        }
                    }
                }
            }

            for (i = 0; i < gAudioContext.audioBufferParameters.numSequencePlayers; i++) {
                SequencePlayer* seqPlayer = &gAudioContext.seqPlayers[i];

                seqPlayer->muted = 0;
                seqPlayer->recalculateVolume = 1;
            }
            break;

        case 0xF3:
            AudioLoad_SyncLoadInstrument(cmd->arg0, cmd->arg1, cmd->arg2);
            break;

        case 0xF4:
            AudioLoad_AsyncLoadSampleBank(cmd->arg0, cmd->arg1, cmd->arg2, &gAudioContext.externalLoadQueue);
            break;

        case 0xF5:
            AudioLoad_AsyncLoadFont(cmd->arg0, cmd->arg1, cmd->arg2, &gAudioContext.externalLoadQueue);
            break;

        case 0xFC:
            AudioLoad_AsyncLoadSeq(cmd->arg0, cmd->arg1, cmd->arg2, &gAudioContext.externalLoadQueue);
            break;

        case 0xF6:
            AudioLoad_DiscardSeqFonts(cmd->arg1);
            break;

        case 0x90:
            gAudioContext.unk_5BDC[cmd->arg0] = cmd->asUShort;
            break;

        case 0xF9:
            gAudioContext.resetStatus = 5;
            gAudioContext.audioResetSpecIdToLoad = cmd->asUInt;
            break;

        case 0xFB:
            D_801755D0 = (void (*)(void))cmd->asUInt;
            break;

        case 0xE0:
        case 0xE1:
        case 0xE2:
            Audio_SetFontInstrument(cmd->op - 0xE0, cmd->arg0, cmd->arg1, cmd->data);
            break;

        case 0xFE:
            temp_t7 = cmd->asUInt;
            if (temp_t7 == 1) {
                for (i = 0; i < gAudioContext.audioBufferParameters.numSequencePlayers; i++) {
                    SequencePlayer* seqPlayer = &gAudioContext.seqPlayers[i];

                    if (seqPlayer->enabled) {
                        AudioSeq_SequencePlayerDisableAsFinished(seqPlayer);
                    }
                }
            }
            func_800E66C0(temp_t7);
            break;

        case 0xE3:
            AudioHeap_PopCache(cmd->asInt);
            break;

        default:
            break;
    }
}

// SetFadeOutTimer
void func_800E5958(s32 playerIdx, s32 fadeTimer) {
    SequencePlayer* seqPlayer = &gAudioContext.seqPlayers[playerIdx];

    if (fadeTimer == 0) {
        fadeTimer = 1;
    }

    seqPlayer->fadeVelocity = -(seqPlayer->fadeVolume / fadeTimer);
    seqPlayer->state = 2;
    seqPlayer->fadeTimer = fadeTimer;
}

// SetFadeInTimer
void func_800E59AC(s32 playerIdx, s32 fadeTimer) {
    SequencePlayer* seqPlayer;

    if (fadeTimer != 0) {
        seqPlayer = &gAudioContext.seqPlayers[playerIdx];
        seqPlayer->state = 1;
        seqPlayer->fadeTimerUnkEu = fadeTimer;
        seqPlayer->fadeTimer = fadeTimer;
        seqPlayer->fadeVolume = 0.0f;
        seqPlayer->fadeVelocity = 0.0f;
    }
}

void Audio_InitMesgQueuesInternal(void) {
    gAudioContext.cmdWrPos = 0;
    gAudioContext.cmdRdPos = 0;
    gAudioContext.cmdQueueFinished = 0;
    gAudioContext.taskStartQueueP = &gAudioContext.taskStartQueue;
    gAudioContext.cmdProcQueueP = &gAudioContext.cmdProcQueue;
    gAudioContext.audioResetQueueP = &gAudioContext.audioResetQueue;
    osCreateMesgQueue(gAudioContext.taskStartQueueP, gAudioContext.taskStartMsgBuf,
                      ARRAY_COUNT(gAudioContext.taskStartMsgBuf));
    osCreateMesgQueue(gAudioContext.cmdProcQueueP, gAudioContext.cmdProcMsgBuf,
                      ARRAY_COUNT(gAudioContext.cmdProcMsgBuf));
    osCreateMesgQueue(gAudioContext.audioResetQueueP, gAudioContext.audioResetMsgBuf,
                      ARRAY_COUNT(gAudioContext.audioResetMsgBuf));
}

void Audio_QueueCmd(u32 opArgs, void** data) {
    AudioCmd* cmd = &gAudioContext.cmdBuf[gAudioContext.cmdWrPos & 0xFF];

    cmd->opArgs = opArgs;
    cmd->data = *data;

    gAudioContext.cmdWrPos++;

    if (gAudioContext.cmdWrPos == gAudioContext.cmdRdPos) {
        gAudioContext.cmdWrPos--;
    }
}

void Audio_QueueCmdF32(u32 opArgs, f32 data) {
    Audio_QueueCmd(opArgs, (void**)&data);
}

void Audio_QueueCmdS32(u32 opArgs, s32 data) {
    Audio_QueueCmd(opArgs, (void**)&data);
}

void Audio_QueueCmdS8(u32 opArgs, s8 data) {
    u32 uData = data << 0x18;

    Audio_QueueCmd(opArgs, (void**)&uData);
}

void Audio_QueueCmdU16(u32 opArgs, u16 data) {
    u32 uData = data << 0x10;

    Audio_QueueCmd(opArgs, (void**)&uData);
}

s32 Audio_ScheduleProcessCmds(void) {
    static s32 D_801304E8 = 0;
    s32 ret;

    if (D_801304E8 < (u8)((gAudioContext.cmdWrPos - gAudioContext.cmdRdPos) + 0x100)) {
        D_801304E8 = (u8)((gAudioContext.cmdWrPos - gAudioContext.cmdRdPos) + 0x100);
    }

    ret =
        osSendMesg(gAudioContext.cmdProcQueueP,
                   (OSMesg)(((gAudioContext.cmdRdPos & 0xFF) << 8) | (gAudioContext.cmdWrPos & 0xFF)), OS_MESG_NOBLOCK);
    if (ret != -1) {
        gAudioContext.cmdRdPos = gAudioContext.cmdWrPos;
        ret = 0;
    } else {
        return -1;
    }

    return ret;
}

void Audio_ResetCmdQueue(void) {
    gAudioContext.cmdQueueFinished = 0;
    gAudioContext.cmdRdPos = gAudioContext.cmdWrPos;
}

void Audio_ProcessCmd(AudioCmd* cmd) {
    SequencePlayer* seqPlayer;
    u16 phi_v0;
    s32 i;

    if ((cmd->op & 0xF0) == 0xF0) {
        func_800E5584(cmd);
        return;
    }

    if (cmd->arg0 < gAudioContext.audioBufferParameters.numSequencePlayers) {
        seqPlayer = &gAudioContext.seqPlayers[cmd->arg0];
        if (cmd->op & 0x80) {
            func_800E5584(cmd);
            return;
        }
        if (cmd->op & 0x40) {
            func_800E6128(seqPlayer, cmd);
            return;
        }

        if (cmd->arg1 < 0x10) {
            func_800E6300(seqPlayer->channels[cmd->arg1], cmd);
            return;
        }
        if (cmd->arg1 == 0xFF) {
            phi_v0 = gAudioContext.unk_5BDC[cmd->arg0];
            for (i = 0; i < 16; i++) {
                if (phi_v0 & 1) {
                    func_800E6300(seqPlayer->channels[i], cmd);
                }
                phi_v0 = phi_v0 >> 1;
            }
        }
    }
}

void Audio_ProcessCmds(u32 msg) {
    static u8 curCmdRdPos = 0;
    AudioCmd* cmd;
    u8 endPos;

    if (!gAudioContext.cmdQueueFinished) {
        curCmdRdPos = msg >> 8;
    }

    while (true) {
        endPos = msg & 0xFF;
        if (curCmdRdPos == endPos) {
            gAudioContext.cmdQueueFinished = 0;
            return;
        }

        cmd = &gAudioContext.cmdBuf[curCmdRdPos++ & 0xFF];
        if (cmd->op == 0xF8) {
            gAudioContext.cmdQueueFinished = 1;
            return;
        }

        Audio_ProcessCmd(cmd);
        cmd->op = 0;
    }
}

u32 func_800E5E20(u32* out) {
    u32 sp1C;

    if (osRecvMesg(&gAudioContext.externalLoadQueue, (OSMesg*)&sp1C, OS_MESG_NOBLOCK) == -1) {
        *out = 0;
        return 0;
    }
    *out = sp1C & 0xFFFFFF;
    return sp1C >> 0x18;
}

u8* func_800E5E84(s32 arg0, u32* arg1) {
    return AudioLoad_GetFontsForSequence(arg0, arg1);
}

void func_800E5EA4(s32 arg0, u32* arg1, u32* arg2) {
    *arg1 = gAudioContext.soundFonts[arg0].sampleBankId1;
    *arg2 = gAudioContext.soundFonts[arg0].sampleBankId2;
}

s32 func_800E5EDC(void) {
    s32 pad;
    s32 sp18;

    if (osRecvMesg(gAudioContext.audioResetQueueP, (OSMesg*)&sp18, OS_MESG_NOBLOCK) == -1) {
        return 0;
    } else if (gAudioContext.audioResetSpecIdToLoad != sp18) {
        return -1;
    } else {
        return 1;
    }
}

void func_800E5F34(void) {
    // macro?
    // clang-format off
    s32 chk = -1; OSMesg msg; do {} while (osRecvMesg(gAudioContext.audioResetQueueP, &msg, OS_MESG_NOBLOCK) != chk);
    // clang-format on
}

s32 func_800E5F88(s32 resetPreloadID) {
    s32 resetStatus;
    OSMesg msg;
    s32 pad;

    func_800E5F34();
    resetStatus = gAudioContext.resetStatus;
    if (resetStatus != 0) {
        Audio_ResetCmdQueue();
        if (gAudioContext.audioResetSpecIdToLoad == resetPreloadID) {
            return -2;
        } else if (resetStatus > 2) {
            gAudioContext.audioResetSpecIdToLoad = resetPreloadID;
            return -3;
        } else {
            osRecvMesg(gAudioContext.audioResetQueueP, &msg, OS_MESG_BLOCK);
        }
    }

    func_800E5F34();
    Audio_QueueCmdS32(0xF9000000, resetPreloadID);

    return Audio_ScheduleProcessCmds();
}

void Audio_PreNMIInternal(void) {
    gAudioContext.resetTimer = 1;
    if (gAudioContextInitalized) {
        func_800E5F88(0);
        gAudioContext.resetStatus = 0;
    }
}

s8 func_800E6070(s32 playerIdx, s32 channelIdx, s32 scriptIdx) {
    SequencePlayer* seqPlayer = &gAudioContext.seqPlayers[playerIdx];
    SequenceChannel* channel;

    if (seqPlayer->enabled) {
        channel = seqPlayer->channels[channelIdx];
        return channel->soundScriptIO[scriptIdx];
    } else {
        return -1;
    }
}

s8 func_800E60C4(s32 playerIdx, s32 port) {
    return gAudioContext.seqPlayers[playerIdx].soundScriptIO[port];
}

void Audio_InitExternalPool(void* ramAddr, u32 size) {
    AudioHeap_AllocPoolInit(&gAudioContext.externalPool, ramAddr, size);
}

void Audio_DestroyExternalPool(void) {
    gAudioContext.externalPool.startRamAddr = NULL;
}

void func_800E6128(SequencePlayer* seqPlayer, AudioCmd* cmd) {
    f32 fadeVolume;

    switch (cmd->op) {
        case 0x41:
            if (seqPlayer->fadeVolumeScale != cmd->asFloat) {
                seqPlayer->fadeVolumeScale = cmd->asFloat;
                seqPlayer->recalculateVolume = 1;
            }
            break;

        case 0x47:
            seqPlayer->tempo = cmd->asInt * 0x30;
            break;

        case 0x49:
            seqPlayer->unk_0C = cmd->asInt * 0x30;
            break;

        case 0x4E:
            seqPlayer->unk_0C = cmd->asInt;
            break;

        case 0x48:
            seqPlayer->transposition = cmd->asSbyte;
            break;

        case 0x46:
            seqPlayer->soundScriptIO[cmd->arg2] = cmd->asSbyte;
            break;

        case 0x4A:
            fadeVolume = (s32)cmd->arg1 / 127.0f;
            goto block_11;

        case 0x4B:
            fadeVolume = ((s32)cmd->arg1 / 100.0f) * seqPlayer->fadeVolume;
        block_11:
            if (seqPlayer->state != 2) {
                seqPlayer->volume = seqPlayer->fadeVolume;
                if (cmd->asInt == 0) {
                    seqPlayer->fadeVolume = fadeVolume;
                } else {
                    s32 fadeTimer = cmd->asInt;

                    seqPlayer->state = 0;
                    seqPlayer->fadeTimer = fadeTimer;
                    seqPlayer->fadeVelocity = (fadeVolume - seqPlayer->fadeVolume) / fadeTimer;
                }
            }
            break;

        case 0x4C:
            if (seqPlayer->state != 2) {
                if (cmd->asInt == 0) {
                    seqPlayer->fadeVolume = seqPlayer->volume;
                } else {
                    s32 fadeTimer = cmd->asInt;

                    seqPlayer->state = 0;
                    seqPlayer->fadeTimer = fadeTimer;
                    seqPlayer->fadeVelocity = (seqPlayer->volume - seqPlayer->fadeVolume) / fadeTimer;
                }
            }
            break;

        case 0x4D:
            seqPlayer->bend = cmd->asFloat;
            if (seqPlayer->bend == 1.0f) {
                seqPlayer->applyBend = false;
            } else {
                seqPlayer->applyBend = true;
            }
            break;

        default:
            break;
    }
}

void func_800E6300(SequenceChannel* channel, AudioCmd* cmd) {
    switch (cmd->op) {
        case CHAN_UPD_VOL_SCALE:
            if (channel->volumeScale != cmd->asFloat) {
                channel->volumeScale = cmd->asFloat;
                channel->changes.s.volume = 1;
            }
            break;

        case CHAN_UPD_VOL:
            if (channel->volume != cmd->asFloat) {
                channel->volume = cmd->asFloat;
                channel->changes.s.volume = 1;
            }
            break;

        case CHAN_UPD_PAN_SIGNED:
            if (channel->newPan != cmd->asSbyte) {
                channel->newPan = cmd->asSbyte;
                channel->changes.s.pan = 1;
            }
            break;

        case CHAN_UPD_PAN_UNSIGNED:
            if (channel->newPan != cmd->asSbyte) {
                channel->panChannelWeight = cmd->asSbyte;
                channel->changes.s.pan = 1;
            }
            break;

        case CHAN_UPD_FREQ_SCALE:
            if (channel->freqScale != cmd->asFloat) {
                channel->freqScale = cmd->asFloat;
                channel->changes.s.freqScale = 1;
            }
            break;

        case CHAN_UPD_REVERB:
            if (channel->reverb != cmd->asSbyte) {
                channel->reverb = cmd->asSbyte;
            }
            break;

        case CHAN_UPD_SCRIPT_IO:
            if (cmd->arg2 < 8) {
                channel->soundScriptIO[cmd->arg2] = cmd->asSbyte;
            }
            break;

        case CHAN_UPD_STOP_SOMETHING2:
            channel->stopSomething2 = cmd->asSbyte;
            break;

        case CHAN_UPD_MUTE_BEHAVE:
            channel->muteBehavior = cmd->asSbyte;
            break;

        case CHAN_UPD_VIBE_X8:
            channel->vibratoExtentTarget = cmd->asUbyte * 8;
            channel->vibratoExtentChangeDelay = 1;
            break;

        case CHAN_UPD_VIBE_X32:
            channel->vibratoRateTarget = cmd->asUbyte * 32;
            channel->vibratoRateChangeDelay = 1;
            break;

        case CHAN_UPD_UNK_0F:
            channel->unk_0F = cmd->asUbyte;
            break;

        case CHAN_UPD_UNK_20:
            channel->unk_20 = cmd->asUShort;
            break;

        case CHAN_UPD_STEREO:
            channel->stereo.asByte = cmd->asUbyte;
            break;

        default:
            break;
    }
}

void func_800E64B0(s32 arg0, s32 arg1, s32 arg2) {
    Audio_QueueCmdS32(((arg0 & 0xFF) << 0x10) | 0xFA000000 | ((arg1 & 0xFF) << 8) | (arg2 & 0xFF), 1);
}

void func_800E64F8(void) {
    Audio_QueueCmdS32(0xFA000000, 0);
}

void func_800E651C(u32 arg0, s32 arg1) {
    Audio_QueueCmdS32((arg1 & 0xFF) | 0xFD000000, arg0);
}

void Audio_WaitForAudioTask(void) {
    osRecvMesg(gAudioContext.taskStartQueueP, NULL, OS_MESG_NOBLOCK);
    osRecvMesg(gAudioContext.taskStartQueueP, NULL, OS_MESG_BLOCK);
}

s32 func_800E6590(s32 playerIdx, s32 arg1, s32 arg2) {
    SequencePlayer* seqPlayer;
    SequenceLayer* layer;
    Note* note;
    SoundFontSound* sound;
    s32 loopEnd;
    s32 samplePos;

    seqPlayer = &gAudioContext.seqPlayers[playerIdx];
    if (seqPlayer->enabled && seqPlayer->channels[arg1]->enabled) {
        layer = seqPlayer->channels[arg1]->layers[arg2];
        if (layer == NULL) {
            return 0;
        }

        if (layer->enabled) {
            if (layer->note == NULL) {
                return 0;
            }

            if (!layer->bit3) {
                return 0;
            }

            note = layer->note;
            if (layer == note->playbackState.parentLayer) {
                sound = note->noteSubEu.sound.soundFontSound;
                if (sound == NULL) {
                    return 0;
                }
                loopEnd = sound->sample->loop->end;
                samplePos = note->synthesisState.samplePosInt;
                return loopEnd - samplePos;
            }
            return 0;
        }
    }
    return 0;
}

s32 func_800E6680(void) {
    return func_800E66C0(0);
}

void func_800E66A0(void) {
    func_800E66C0(2);
}

s32 func_800E66C0(s32 arg0) {
    s32 phi_v1;
    NotePlaybackState* temp_a2;
    NoteSubEu* temp_a3;
    s32 i;
    Note* note;
    SoundFontSound* sound;

    phi_v1 = 0;
    for (i = 0; i < gAudioContext.numNotes; i++) {
        note = &gAudioContext.notes[i];
        temp_a2 = &note->playbackState;
        if (note->noteSubEu.bitField0.enabled) {
            temp_a3 = &note->noteSubEu;
            if (temp_a2->adsr.action.s.state != 0) {
                if (arg0 >= 2) {
                    sound = temp_a3->sound.soundFontSound;
                    if (sound == NULL || temp_a3->bitField1.isSyntheticWave) {
                        continue;
                    }
                    if (sound->sample->medium == MEDIUM_RAM) {
                        continue;
                    }
                }

                phi_v1++;
                if ((arg0 & 1) == 1) {
                    temp_a2->adsr.fadeOutVel = gAudioContext.audioBufferParameters.updatesPerFrameInv;
                    temp_a2->adsr.action.s.release = 1;
                }
            }
        }
    }
    return phi_v1;
}

u32 Audio_NextRandom(void) {
    static u32 audRand = 0x12345678;

    audRand = ((osGetCount() + 0x1234567) * (audRand + gAudioContext.totalTaskCount));
    audRand += gAudioContext.audioRandom;
    return audRand;
}

void Audio_InitMesgQueues(void) {
    Audio_InitMesgQueuesInternal();
}
