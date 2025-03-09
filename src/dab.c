#include "modding.h"
#include "global.h"

extern Input* sPlayerControlInput;
void Player_Action_18(Player* this, PlayState* play);
extern PlayerAnimationHeader* D_8085BE84[PLAYER_ANIMGROUP_MAX][PLAYER_ANIMTYPE_MAX];
extern LinkAnimationHeader gPlayerAnim_pn_gurd;
extern LinkAnimationHeader gPlayerAnim_clink_normal_defense_ALL;
void func_8082DC38(Player* this);
void Player_DetachHeldActor(PlayState* play, Player* this);
s32 Player_SetAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
void func_8082FC60(Player* this);
void func_80830AE8(Player* this);
AnimTask* AnimTaskQueue_NewTask(AnimTaskQueue* animTaskQueue, AnimTaskType type);
extern LinkAnimationHeader gPlayerAnim_pz_wait;
extern LinkAnimationHeader gPlayerAnim_pg_wait;
extern LinkAnimationHeader gPlayerAnim_cl_msbowait;

#define LINK_ANIMETION_OFFSET(addr, offset) \
    (SEGMENT_ROM_START(link_animetion) + ((uintptr_t)addr & 0xFFFFFF) + ((u32)offset))

RECOMP_IMPORT("*", int recomp_printf(const char* fmt, ...));

extern LinkAnimationHeader gLinkHumanSkelEpicdabAnim;

RECOMP_PATCH void AnimTaskQueue_AddLoadPlayerFrame(PlayState* play, PlayerAnimationHeader* animation, s32 frame, s32 limbCount,
                                      Vec3s* frameTable) {
    AnimTask* task = AnimTaskQueue_NewTask(&play->animTaskQueue, ANIMTASK_LOAD_PLAYER_FRAME);

    if (task != NULL) {
        PlayerAnimationHeader* playerAnimHeader = Lib_SegmentedToVirtual(animation);
        s32 pad;

        osCreateMesgQueue(&task->data.loadPlayerFrame.msgQueue, task->data.loadPlayerFrame.msg,
                          ARRAY_COUNT(task->data.loadPlayerFrame.msg));
        
        // recomp_printf("Requesting frame for animation %08X (%08X)\n", (u32)playerAnimHeader, (u32)playerAnimHeader->linkAnimSegment);
        if (IS_KSEG0(playerAnimHeader->linkAnimSegment)) {
            // recomp_printf("  KSEG0 animation\n");
            osSendMesg(&task->data.loadPlayerFrame.msgQueue, NULL, OS_MESG_NOBLOCK);
            Lib_MemCpy(frameTable, ((u8*)playerAnimHeader->segmentVoid) + (sizeof(Vec3s) * limbCount + sizeof(s16)) * frame,
                sizeof(Vec3s) * limbCount + sizeof(s16));
        }
        else {
            // recomp_printf(" ROM animation\n");
            DmaMgr_RequestAsync(
                &task->data.loadPlayerFrame.req, frameTable,
                LINK_ANIMETION_OFFSET(playerAnimHeader->linkAnimSegment, (sizeof(Vec3s) * limbCount + sizeof(s16)) * frame),
                sizeof(Vec3s) * limbCount + sizeof(s16), 0, &task->data.loadPlayerFrame.msgQueue, NULL);
        }
    }
}

// Debug feature to test the dab animation with L
/*
RECOMP_PATCH PlayerAnimationHeader* Player_GetIdleAnim(Player* this) {
    if ((this->transformation == PLAYER_FORM_ZORA) || (this->actor.id != ACTOR_PLAYER)) {
        return &gPlayerAnim_pz_wait;
    }
    if (this->transformation == PLAYER_FORM_GORON) {
        return &gPlayerAnim_pg_wait;
    }
    if (this->currentMask == PLAYER_MASK_SCENTS) {
        return &gPlayerAnim_cl_msbowait;
    }
    if (sPlayerControlInput != NULL && (this->transformation == PLAYER_FORM_HUMAN || this->transformation == PLAYER_FORM_HUMAN) && CHECK_BTN_ALL(sPlayerControlInput->cur.button, BTN_L)) {
        return &gLinkHumanSkelEpicdabAnim;
    }
    return D_8085BE84[PLAYER_ANIMGROUP_wait][this->modelAnimType];
}*/

extern s32 Player_SetAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
extern EquipSlot func_8082FDC4(void);
extern ItemId Player_GetItemOnButton(PlayState* play, Player* player, EquipSlot slot);
extern void Player_Anim_PlayOnce(PlayState* play, Player* this, PlayerAnimationHeader* anim);
extern void Player_Anim_PlayLoop(PlayState* play, Player* this, PlayerAnimationHeader* anim);
extern s32 PlayerAnimation_Update(PlayState* play, SkelAnime* skelAnime);
extern void Player_Action_Idle(Player* this, PlayState* play);
extern void PlayerAnimation_AnimateFrame(PlayState* play, SkelAnime* skelAnime);
extern void Player_SetAction_PreserveItemAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 arg3);
void Player_Action_Dab(Player* this, PlayState* play) {
    Player_Anim_PlayOnce(play, this, &gLinkHumanSkelEpicdabAnim); 
    //s32 animFinished = PlayerAnimation_Update(play, &this->skelAnime);
    //if (animFinished) {
        Player_SetAction(play, this, Player_Action_Idle,1);
    //}
}

PlayState* sPlayState;

RECOMP_CALLBACK("*", recomp_on_play_init)
void OnPlayInit(PlayState* play) {
    sPlayState = play;
}

RECOMP_HOOK("Player_ProcessItemButtons") void on_Player_ProcessItemButtons(Player* this, PlayState* play) {
    EquipSlot i = func_8082FDC4();
        i = ((i >= EQUIP_SLOT_A) && (this->transformation == PLAYER_FORM_FIERCE_DEITY) &&
             (this->heldItemAction != PLAYER_IA_SWORD_TWO_HANDED))
                ? EQUIP_SLOT_B
                : i;
    if (Player_GetItemOnButton(play, this, i) == ITEM_F0) {
        if (this->blastMaskTimer == 0) {
            this->invincibilityTimer = 20;
            Player_SetAction_PreserveItemAction(play, this, Player_Action_Dab, 1);
        }

    }
}

RECOMP_HOOK_RETURN("Player_ProcessItemButtons")
void BlastMaskCooldown_Player_ProcessItemButtons() {
    Player* player = GET_PLAYER(sPlayState);

    if (player->blastMaskTimer != 310) {
        return;
    }

    player->blastMaskTimer = 10;
}

extern Gfx* D_801C0B20[];
extern Gfx sunglasses_sunglasses_mesh[];

RECOMP_HOOK("Player_Init") void on_Player_Init() {
    D_801C0B20[17] = sunglasses_sunglasses_mesh;
}
extern u32 gSegments[NUM_SEGMENTS];
extern void AnimatedMat_DrawOpa(PlayState* play, AnimatedMaterial* matAnim);

RECOMP_PATCH void Player_DrawBlastMask(PlayState* play, Player* player) {
    static Gfx D_801C0BC0[] = {
        gsDPSetEnvColor(0, 0, 0, 255),
        gsSPEndDisplayList(),
    };
    static Gfx D_801C0BD0[] = {
        gsDPSetRenderMode(AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              G_RM_FOG_SHADE_A,
                          AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL |
                              GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA)),
        gsSPEndDisplayList(),
    };

    OPEN_DISPS(play->state.gfxCtx);

    if (1) {
        s32 alpha;

        gSegments[0x0A] = OS_K0_TO_PHYSICAL(player->maskObjectSegment);

        if (player->blastMaskTimer <= 10) {
            alpha = (player->blastMaskTimer / 10.0f) * 255;
        } else {
            alpha = 255;
        }

        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, (u8)alpha);
        gSPDisplayList(POLY_OPA_DISP++, sunglasses_sunglasses_mesh);
        gSPSegment(POLY_OPA_DISP++, 0x09, D_801C0BD0);
        gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, (u8)(255 - alpha));
    } else {
        gSPSegment(POLY_OPA_DISP++, 0x09, D_801C0BC0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
extern u64 dab_dabPromptSD_ia4[];

u8 gBActionIsExplode = 0;
PlayState* gPlayState;

RECOMP_HOOK("Interface_SetBButtonPlayerDoAction") void on_Interface_SetBButtonPlayerDoAction(PlayState* play, s16 bButtonDoAction) {
    if (bButtonDoAction == DO_ACTION_EXPLODE) {
        gBActionIsExplode = 1;
    } else {
        gBActionIsExplode = 0;
    }
    gPlayState = play;
}

RECOMP_HOOK_RETURN("Interface_SetBButtonPlayerDoAction") void return_Interface_SetBButtonPlayerDoAction(PlayState* play, s16 bButtonDoAction) {
    if (gBActionIsExplode) {
        Lib_MemCpy(gPlayState->interfaceCtx.doActionSegment + DO_ACTION_OFFSET_B_INTERFACE, dab_dabPromptSD_ia4, DO_ACTION_TEX_SIZE);
    }
    gBActionIsExplode = 0;
}

extern u64 dab_shadesHD_rgba32[];
u8 gCurMaskIsBlast = 0;
PlayState* gPlayState2;
u8 gId;
void* gDst;
u8 gAlreadyTranslated = 0;
uintptr_t gTranslatedAddress;

RECOMP_HOOK("CmpDma_LoadFileImpl") void on_CmpDma_LoadFileImpl(uintptr_t segmentRom, s32 id, void* dst, size_t size) {
    if (!gAlreadyTranslated ) {
        gTranslatedAddress = DmaMgr_TranslateVromToRom(SEGMENT_ROM_START(icon_item_static_yar));
        gAlreadyTranslated = 1;
    }
    if (id == ITEM_MASK_BLAST && segmentRom == gTranslatedAddress) {
        recomp_printf("Blast mask\n");
        gCurMaskIsBlast = 1;
    } else {
        gCurMaskIsBlast = 0;
        recomp_printf("Item: %d\n", id);
    } 
    gId = id;
    gDst = dst;
}

RECOMP_HOOK_RETURN("CmpDma_LoadFileImpl") void return_CmpDma_LoadFileImpl(void) {
    if (gCurMaskIsBlast) {
        Lib_MemCpy(gDst, dab_shadesHD_rgba32, ICON_ITEM_TEX_SIZE);
    }
    gCurMaskIsBlast = 0;
}