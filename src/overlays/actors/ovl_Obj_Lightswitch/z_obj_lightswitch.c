/*
 * File: z_obj_lightswitch.c
 * Overlay: ovl_Obj_Lightswitch
 * Description: Sun Emblem Trigger (Spirit Temple)
 */

#include "z_obj_lightswitch.h"
#include "vt.h"
#include "overlays/actors/ovl_Obj_Oshihiki/z_obj_oshihiki.h"
#include "assets/objects/object_lightswitch/object_lightswitch.h"

#define FLAGS ACTOR_FLAG_4

typedef enum {
    /* 0x00 */ FACE_EYES_CLOSED,
    /* 0x01 */ FACE_EYES_OPEN,
    /* 0x02 */ FACE_EYES_OPEN_SMILING
} FaceTextureIndex;

void ObjLightswitch_Init(Actor* thisx, PlayState* play);
void ObjLightswitch_Destroy(Actor* thisx, PlayState* play);
void ObjLightswitch_Update(Actor* thisx, PlayState* play);
void ObjLightswitch_Draw(Actor* thisx, PlayState* play);

void ObjLightswitch_SetupOff(ObjLightswitch* this);
void ObjLightswitch_Off(ObjLightswitch* this, PlayState* play);
void ObjLightswitch_SetupTurnOn(ObjLightswitch* this);
void ObjLightswitch_TurnOn(ObjLightswitch* this, PlayState* play);
void ObjLightswitch_SetupOn(ObjLightswitch* this);
void ObjLightswitch_On(ObjLightswitch* this, PlayState* play);
void ObjLightswitch_SetupTurnOff(ObjLightswitch* this);
void ObjLightswitch_TurnOff(ObjLightswitch* this, PlayState* play);
void ObjLightswitch_SetupDisappearDelay(ObjLightswitch* this);
void ObjLightswitch_DisappearDelay(ObjLightswitch* this, PlayState* play);
void ObjLightswitch_SetupDisappear(ObjLightswitch* this);
void ObjLightswitch_Disappear(ObjLightswitch* this, PlayState* play);

const ActorInit Obj_Lightswitch_InitVars = {
    ACTOR_OBJ_LIGHTSWITCH,
    ACTORCAT_SWITCH,
    FLAGS,
    OBJECT_LIGHTSWITCH,
    sizeof(ObjLightswitch),
    (ActorFunc)ObjLightswitch_Init,
    (ActorFunc)ObjLightswitch_Destroy,
    (ActorFunc)ObjLightswitch_Update,
    (ActorFunc)ObjLightswitch_Draw,
};

static ColliderJntSphElementInit sColliderJntSphElementInit[] = {
    {
        {
            ELEMTYPE_UNK0,
            { 0x00000000, 0x00, 0x00 },
            { 0x00200000, 0x00, 0x00 },
            TOUCH_NONE,
            BUMP_ON,
            OCELEM_ON,
        },
        { 0, { { 0, 0, 0 }, 19 }, 100 },
    },
};
static ColliderJntSphInit sColliderJntSphInit = {
    {
        COLTYPE_NONE,
        AT_NONE,
        AC_ON | AC_TYPE_PLAYER,
        OC1_ON | OC1_TYPE_ALL,
        OC2_TYPE_2,
        COLSHAPE_JNTSPH,
    },
    1,
    sColliderJntSphElementInit,
};

static CollisionCheckInfoInit sColChkInfoInit = { 0, 12, 60, MASS_IMMOVABLE };

static void* sFaceTextures[] = { object_lightswitch_Tex_000C20, object_lightswitch_Tex_000420,
                                 object_lightswitch_Tex_001420 };

static Vec3f D_80B97F68 = { -1707.0f, 843.0f, -180.0f };
static Vec3f D_80B97F74 = { 0.0f, 0.0f, 0.0f };

static InitChainEntry sInitChain[] = {
    ICHAIN_VEC3F_DIV1000(scale, 100, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneForward, 1000, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneScale, 500, ICHAIN_CONTINUE),
    ICHAIN_F32(uncullZoneDownward, 1000, ICHAIN_STOP),
};

void ObjLightswitch_InitCollider(ObjLightswitch* this, PlayState* play) {
    s32 pad;

    Collider_InitJntSph(play, &this->collider);
    Collider_SetJntSph(play, &this->collider, &this->actor, &sColliderJntSphInit, this->colliderItems);
    Matrix_SetTranslateRotateYXZ(this->actor.world.pos.x,
                                 this->actor.world.pos.y + (this->actor.shape.yOffset * this->actor.scale.y),
                                 this->actor.world.pos.z, &this->actor.shape.rot);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    Collider_UpdateSpheres(0, &this->collider);
}

void ObjLightswitch_SetSwitchFlag(ObjLightswitch* this, PlayState* play) {
    Actor* thisx = &this->actor; // required
    s32 type;

    if (!Flags_GetSwitch(play, this->actor.params >> 8 & 0x3F)) {
        type = this->actor.params >> 4 & 3;

        Flags_SetSwitch(play, this->actor.params >> 8 & 0x3F);

        if (type == OBJLIGHTSWITCH_TYPE_1) {
            OnePointCutscene_AttentionSetSfx(play, thisx, NA_SE_SY_TRE_BOX_APPEAR);
        } else if (type == OBJLIGHTSWITCH_TYPE_BURN) {
            OnePointCutscene_AttentionSetSfx(play, thisx, NA_SE_SY_ERROR);
        } else {
            OnePointCutscene_AttentionSetSfx(play, thisx, NA_SE_SY_CORRECT_CHIME);
        }
    }
}

void ObjLightswitch_ClearSwitchFlag(ObjLightswitch* this, PlayState* play) {
    if (Flags_GetSwitch(play, this->actor.params >> 8 & 0x3F)) {
        Flags_UnsetSwitch(play, this->actor.params >> 8 & 0x3F);

        if ((this->actor.params >> 4 & 3) == OBJLIGHTSWITCH_TYPE_1) {
            OnePointCutscene_AttentionSetSfx(play, &this->actor, NA_SE_SY_TRE_BOX_APPEAR);
        }
    }
}

void ObjLightswitch_SpawnDisappearEffects(ObjLightswitch* this, PlayState* play) {
    Vec3f pos;
    f32 s = Math_SinS(this->actor.shape.rot.y);
    f32 c = Math_CosS(this->actor.shape.rot.y);
    f32 x;
    f32 y;
    f32 z;
    s32 pad;

    if (this->alpha >= (100 << 6)) {
        x = (CLAMP_MAX((1.0f - 1.0f / (255 << 6) * this->alpha) * 400.0f, 60.0f) - 30.0f + 30.0f) * Rand_ZeroOne();
        y = x - 30.0f;
        if (x > 30.0f) {
            x = 30.0f;
        } else {
            x = 900.0f - (y * y);
            if (x < 100.0f) {
                x = 100.0f;
            }
            x = sqrtf(x);
        }
        x = 2.0f * (x * (Rand_ZeroOne() - 0.5f));
        z = (30.0f - fabsf(x)) * 0.5f + 10.0f * Rand_ZeroOne();
        pos.x = this->actor.world.pos.x + ((z * s) + (x * c));
        pos.y = this->actor.world.pos.y + y + 10.0f;
        pos.z = this->actor.world.pos.z + ((z * c) - (x * s));
        EffectSsDeadDb_Spawn(play, &pos, &D_80B97F74, &D_80B97F74, 100, 0, 255, 255, 160, 160, 255, 0, 0, 1, 9, true);
    }
}

void ObjLightswitch_Init(Actor* thisx, PlayState* play) {
    ObjLightswitch* this = (ObjLightswitch*)thisx;
    s32 switchFlagSet = Flags_GetSwitch(play, this->actor.params >> 8 & 0x3F);
    s32 removeSelf = false;

    Actor_ProcessInitChain(&this->actor, sInitChain);
    Actor_SetFocus(&this->actor, 0.0f);
    if (switchFlagSet) {
        if ((this->actor.params >> 4 & 3) == OBJLIGHTSWITCH_TYPE_BURN) {
            removeSelf = true;
        } else {
            ObjLightswitch_SetupOn(this);
        }
    } else {
        ObjLightswitch_SetupOff(this);
    }
    if ((this->actor.params & 1) == 1) {
        if (switchFlagSet) {
            Math_Vec3f_Copy(&this->actor.world.pos, &D_80B97F68);
            Math_Vec3f_Copy(&this->actor.home.pos, &D_80B97F68);
        }
        this->actor.shape.rot.x = -0x4000;
        this->actor.shape.rot.z = 0;
        this->actor.world.rot.x = this->actor.home.rot.x = this->actor.shape.rot.x;
        this->actor.world.rot.z = this->actor.home.rot.z = this->actor.shape.rot.z;
        this->actor.flags |= ACTOR_FLAG_5;
        if (Actor_SpawnAsChild(&play->actorCtx, &this->actor, play, ACTOR_OBJ_OSHIHIKI, this->actor.home.pos.x,
                               this->actor.home.pos.y, this->actor.home.pos.z, 0, this->actor.home.rot.y, 0,
                               (0xFF << 8) | PUSHBLOCK_SMALL_START_ON) == NULL) {
            osSyncPrintf(VT_COL(RED, WHITE));
            // "Push-pull block occurrence failure"
            osSyncPrintf("押引ブロック発生失敗(%s %d)(arg_data 0x%04x)\n", "../z_obj_lightswitch.c", 452,
                         this->actor.params);
            osSyncPrintf(VT_RST);
            removeSelf = true;
        }
    }
    ObjLightswitch_InitCollider(this, play);
    CollisionCheck_SetInfo(&this->actor.colChkInfo, NULL, &sColChkInfoInit);
    if (removeSelf) {
        Actor_Kill(&this->actor);
    }
    // "Light switch"
    osSyncPrintf("(光スイッチ)(arg_data 0x%04x)\n", this->actor.params);
}

void ObjLightswitch_Destroy(Actor* thisx, PlayState* play2) {
    PlayState* play = play2;
    ObjLightswitch* this = (ObjLightswitch*)thisx;

    Collider_DestroyJntSph(play, &this->collider);
}

void ObjLightswitch_SetupOff(ObjLightswitch* this) {
    this->actionFunc = ObjLightswitch_Off;
    this->faceTextureIndex = FACE_EYES_CLOSED;
    this->color[0] = 155 << 6;
    this->color[1] = 125 << 6;
    this->color[2] = 255 << 6;
    this->alpha = 255 << 6;
}

void ObjLightswitch_Off(ObjLightswitch* this, PlayState* play) {
    switch (this->actor.params >> 4 & 3) {
        case OBJLIGHTSWITCH_TYPE_STAY_ON:
        case OBJLIGHTSWITCH_TYPE_2:
            if (this->collider.base.acFlags & AC_HIT) {
                ObjLightswitch_SetupTurnOn(this);
                ObjLightswitch_SetSwitchFlag(this, play);
            }
            break;
        case OBJLIGHTSWITCH_TYPE_1:
            if ((this->collider.base.acFlags & AC_HIT) && !(this->prevFrameACflags & AC_HIT)) {
                ObjLightswitch_SetupTurnOn(this);
                ObjLightswitch_SetSwitchFlag(this, play);
            }
            break;
        case OBJLIGHTSWITCH_TYPE_BURN:
            if (this->collider.base.acFlags & AC_HIT) {
                ObjLightswitch_SetupDisappearDelay(this);
                ObjLightswitch_SetSwitchFlag(this, play);
            }
            break;
    }
}

void ObjLightswitch_SetupTurnOn(ObjLightswitch* this) {
    this->actionFunc = ObjLightswitch_TurnOn;
    this->toggleDelay = 100;
    this->timer = 0;
    this->faceTextureIndex = FACE_EYES_CLOSED;
}

void ObjLightswitch_TurnOn(ObjLightswitch* this, PlayState* play) {
    if (func_8005B198() == this->actor.category || this->toggleDelay <= 0) {
        if (this->timer == 0) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_TRIFORCE_FLASH);
        }

        this->timer++;

        Math_StepToS(&this->flameRingRotSpeed, -0xAA, 0xA);
        this->flameRingRot += this->flameRingRotSpeed;

        this->color[0] = this->timer * (((255 - 155) << 6) / 20) + (155 << 6);
        this->color[1] = this->timer * (((255 - 125) << 6) / 20) + (125 << 6);

        if (this->timer >= 20) {
            ObjLightswitch_SetupOn(this);
        } else if (this->timer == 15) {
            this->faceTextureIndex = FACE_EYES_OPEN;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_FOOT_SWITCH);
        }
    }
}

void ObjLightswitch_SetupOn(ObjLightswitch* this) {
    this->actionFunc = ObjLightswitch_On;
    this->faceTextureIndex = FACE_EYES_OPEN_SMILING;

    this->color[0] = 255 << 6;
    this->color[1] = 255 << 6;
    this->color[2] = 255 << 6;
    this->alpha = 255 << 6;

    this->flameRingRotSpeed = -0xAA;
    this->timer = 0;
}

void ObjLightswitch_On(ObjLightswitch* this, PlayState* play) {
    switch (this->actor.params >> 4 & 3) {
        case OBJLIGHTSWITCH_TYPE_STAY_ON:
            if (!Flags_GetSwitch(play, this->actor.params >> 8 & 0x3F)) {
                ObjLightswitch_SetupTurnOff(this);
            }
            break;
        case OBJLIGHTSWITCH_TYPE_1:
            if (this->collider.base.acFlags & AC_HIT && !(this->prevFrameACflags & AC_HIT)) {
                ObjLightswitch_SetupTurnOff(this);
                ObjLightswitch_ClearSwitchFlag(this, play);
            }
            break;
        case OBJLIGHTSWITCH_TYPE_2:
            if (!(this->collider.base.acFlags & AC_HIT)) {
                if (this->timer >= 7) {
                    ObjLightswitch_SetupTurnOff(this);
                    ObjLightswitch_ClearSwitchFlag(this, play);
                } else {
                    this->timer++;
                }
            } else {
                this->timer = 0;
            }
            break;
    }
    this->flameRingRot += this->flameRingRotSpeed;
}

void ObjLightswitch_SetupTurnOff(ObjLightswitch* this) {
    this->actionFunc = ObjLightswitch_TurnOff;
    this->toggleDelay = 100;
    this->timer = 20;
    this->faceTextureIndex = FACE_EYES_OPEN;
}

void ObjLightswitch_TurnOff(ObjLightswitch* this, PlayState* play) {
    if ((this->actor.params >> 4 & 3) != OBJLIGHTSWITCH_TYPE_1 || func_8005B198() == this->actor.category ||
        this->toggleDelay <= 0) {
        this->timer--;

        Math_StepToS(&this->flameRingRotSpeed, 0, 0xA);
        this->flameRingRot += this->flameRingRotSpeed;

        this->color[0] = this->timer * (((255 - 155) << 6) / 20) + (155 << 6);
        this->color[1] = this->timer * (((255 - 125) << 6) / 20) + (125 << 6);

        if (this->timer <= 0) {
            ObjLightswitch_SetupOff(this);
        } else if (this->timer == 15) {
            this->faceTextureIndex = FACE_EYES_CLOSED;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_FOOT_SWITCH);
        }
    }
}

void ObjLightswitch_SetupDisappearDelay(ObjLightswitch* this) {
    this->actionFunc = ObjLightswitch_DisappearDelay;
    this->toggleDelay = 100;
}

void ObjLightswitch_DisappearDelay(ObjLightswitch* this, PlayState* play) {
    if (func_8005B198() == this->actor.category || this->toggleDelay <= 0) {
        ObjLightswitch_SetupDisappear(this);
    }
}

void ObjLightswitch_SetupDisappear(ObjLightswitch* this) {
    this->actionFunc = ObjLightswitch_Disappear;
    this->alpha = 255 << 6;
}

void ObjLightswitch_Disappear(ObjLightswitch* this, PlayState* play) {
    this->alpha -= 200;
    ObjLightswitch_SpawnDisappearEffects(this, play);
    if (this->alpha < 0) {
        Actor_Kill(&this->actor);
    }
}

void ObjLightswitch_Update(Actor* thisx, PlayState* play2) {
    ObjLightswitch* this = (ObjLightswitch*)thisx;
    PlayState* play = play2;

    if (this->toggleDelay > 0) {
        this->toggleDelay--;
    }

    this->actionFunc(this, play);

    if (this->actor.update != NULL) {
        if ((this->actor.params & 1) == 1) {
            this->actor.world.pos.x = this->actor.child->world.pos.x;
            this->actor.world.pos.y = this->actor.child->world.pos.y + 60.0f;
            this->actor.world.pos.z = this->actor.child->world.pos.z;
            Actor_SetFocus(&this->actor, 0.0f);
        }

        this->prevFrameACflags = this->collider.base.acFlags;
        this->collider.base.acFlags &= ~AC_HIT;
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

void ObjLightswitch_DrawOpa(ObjLightswitch* this, PlayState* play) {
    Actor* child;
    Vec3f pos;
    Vec3s rot;

    OPEN_DISPS(play->state.gfxCtx, "../z_obj_lightswitch.c", 809);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);

    gDPSetEnvColor(POLY_OPA_DISP++, (u8)(this->color[0] >> 6), (u8)(this->color[1] >> 6), (u8)(this->color[2] >> 6),
                   (u8)(this->alpha >> 6));
    gSPSegment(POLY_OPA_DISP++, 0x09, &D_80116280[2]);

    if ((this->actor.params & 1) == 1) {
        child = this->actor.child;
        this->actor.world.pos.x = child->world.pos.x;
        this->actor.world.pos.y = child->world.pos.y + 60.0f;
        this->actor.world.pos.z = child->world.pos.z;
        Math_Vec3f_Copy(&pos, &this->actor.world.pos);
        Matrix_SetTranslateRotateYXZ(pos.x, pos.y, pos.z, &this->actor.shape.rot);
        Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    } else {
        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y + this->actor.shape.yOffset * this->actor.scale.y;
        pos.z = this->actor.world.pos.z;
    }

    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, "../z_obj_lightswitch.c", 841),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sFaceTextures[this->faceTextureIndex]));
    gSPDisplayList(POLY_OPA_DISP++, object_lightswitch_DL_000260);

    rot.x = this->actor.shape.rot.x;
    rot.y = this->actor.shape.rot.y;
    rot.z = this->actor.shape.rot.z + this->flameRingRot;
    Matrix_SetTranslateRotateYXZ(pos.x, pos.y, pos.z, &rot);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, "../z_obj_lightswitch.c", 859),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, object_lightswitch_DL_000398);

    rot.z = this->actor.shape.rot.z - this->flameRingRot;
    Matrix_SetTranslateRotateYXZ(pos.x, pos.y, pos.z, &rot);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, Matrix_NewMtx(play->state.gfxCtx, "../z_obj_lightswitch.c", 873),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, object_lightswitch_DL_000408);

    CLOSE_DISPS(play->state.gfxCtx, "../z_obj_lightswitch.c", 878);
}

void ObjLightswitch_DrawXlu(ObjLightswitch* this, PlayState* play) {
    s32 pad;
    Vec3f sp68;
    Vec3s sp60;

    OPEN_DISPS(play->state.gfxCtx, "../z_obj_lightswitch.c", 890);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);

    gDPSetEnvColor(POLY_XLU_DISP++, (u8)(this->color[0] >> 6), (u8)(this->color[1] >> 6), (u8)(this->color[2] >> 6),
                   (u8)(this->alpha >> 6));
    gSPSegment(POLY_XLU_DISP++, 0x09, D_80116280);

    sp68.x = this->actor.world.pos.x;
    sp68.y = this->actor.world.pos.y + (this->actor.shape.yOffset * this->actor.scale.y);
    sp68.z = this->actor.world.pos.z;

    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, "../z_obj_lightswitch.c", 912),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sFaceTextures[this->faceTextureIndex]));
    gSPDisplayList(POLY_XLU_DISP++, object_lightswitch_DL_000260);

    sp60.x = this->actor.shape.rot.x;
    sp60.y = this->actor.shape.rot.y;
    sp60.z = this->actor.shape.rot.z + this->flameRingRot;

    Matrix_SetTranslateRotateYXZ(sp68.x, sp68.y, sp68.z, &sp60);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, "../z_obj_lightswitch.c", 930),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, object_lightswitch_DL_000398);

    sp60.z = this->actor.shape.rot.z - this->flameRingRot;
    Matrix_SetTranslateRotateYXZ(sp68.x, sp68.y, sp68.z, &sp60);
    Matrix_Scale(this->actor.scale.x, this->actor.scale.y, this->actor.scale.z, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, Matrix_NewMtx(play->state.gfxCtx, "../z_obj_lightswitch.c", 944),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, object_lightswitch_DL_000408);

    CLOSE_DISPS(play->state.gfxCtx, "../z_obj_lightswitch.c", 949);
}

void ObjLightswitch_Draw(Actor* thisx, PlayState* play) {
    ObjLightswitch* this = (ObjLightswitch*)thisx;
    s32 alpha = this->alpha >> 6 & 0xFF;

    if ((this->actor.params & 1) == 1) {
        Collider_UpdateSpheres(0, &this->collider);
    }

    if ((this->actor.params >> 4 & 3) == OBJLIGHTSWITCH_TYPE_BURN && (alpha > 0 || alpha < 255)) {
        ObjLightswitch_DrawXlu(this, play);
    } else {
        ObjLightswitch_DrawOpa(this, play);
    }
}
