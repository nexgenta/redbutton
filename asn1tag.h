/*
 * asn1tag.h
 */

#ifndef __ASN1TAG_H__
#define __ASN1TAG_H__

/* tag classes */
#define ASN1CLASS_UNIVERSAL	0x00
#define ASN1CLASS_APPLICATION	0x40
#define ASN1CLASS_CONTEXT	0x80
#define ASN1CLASS_PRIVATE	0xc0

/*
 * a synthetic object created as a result of the grammar definition
 * eg TextBody etc
 * don't output this object, just output its children
 */
#define ASN1TAG_SYNTHETIC	10000
/* the tag for CHOICE types is determined by which choice we choose */
#define ASN1TAG_CHOICE		10001
/* ENUMERATED types are encoded as INTEGERs */
#define ASN1TAG_ENUMERATED	10002

/* abstract types */
#define ASN1TAG_Group		ASN1TAG_SYNTHETIC
#define ASN1TAG_Ingredient	ASN1TAG_SYNTHETIC
#define ASN1TAG_Program		ASN1TAG_SYNTHETIC
#define ASN1TAG_Variable	ASN1TAG_SYNTHETIC
#define ASN1TAG_Visible		ASN1TAG_SYNTHETIC
#define ASN1TAG_Interactible	ASN1TAG_SYNTHETIC
#define ASN1TAG_Button		ASN1TAG_SYNTHETIC
#define ASN1TAG_TokenManager	ASN1TAG_SYNTHETIC

/* tokens synthesised by the grammar */
#define ASN1TAG_TokenGroupBody	ASN1TAG_SYNTHETIC
#define ASN1TAG_LineArtBody	ASN1TAG_SYNTHETIC
#define ASN1TAG_TextBody	ASN1TAG_SYNTHETIC
#define ASN1TAG_PushButtonBody	ASN1TAG_SYNTHETIC


/* start TODO */
#define ASN1TAG_FIXME 99999
/* TODO: need to look at these - have different values in different places */
#define ASN1TAG_ReferencedContent	ASN1TAG_FIXME
#define ASN1TAG_XYPosition		ASN1TAG_FIXME
#define ASN1TAG_Point			ASN1TAG_FIXME
/* TODO: as above, but only used once */
#define ASN1TAG_Rational		ASN1TAG_FIXME
#define ASN1TAG_ExternalReference	ASN1TAG_FIXME
#define ASN1TAG_NewReferencedContent	ASN1TAG_FIXME
#define ASN1TAG_NextScene		ASN1TAG_FIXME
#define ASN1TAG_TokenGroupItem		ASN1TAG_FIXME
/* TODO: an INTEGER ie class=UNIVERSAL, tag=2 */
#define ASN1TAG_Movement	ASN1TAG_FIXME
/* TODO: sequences */
#define ASN1TAG_ActionSlots	ASN1TAG_FIXME
#define ASN1TAG_InVariables	ASN1TAG_FIXME
#define ASN1TAG_OutVariables	ASN1TAG_FIXME
#define ASN1TAG_ActionClass	ASN1TAG_FIXME
#define ASN1TAG_Parameters	ASN1TAG_FIXME
#define ASN1TAG_PointList	ASN1TAG_FIXME
/* end TODO */

/* ASN1 tags */
#define ASN1TAG_ApplicationClass	0
#define ASN1TAG_SceneClass	1
#define ASN1TAG_StandardIdentifier	2
#define ASN1TAG_StandardVersion	3
#define ASN1TAG_ObjectInformation	4
#define ASN1TAG_OnStartUp	5
#define ASN1TAG_OnCloseDown	6
#define ASN1TAG_OriginalGroupCachePriority	7
#define ASN1TAG_Items	8
#define ASN1TAG_ResidentProgramClass	9
#define ASN1TAG_RemoteProgramClass	10
#define ASN1TAG_InterchangedProgramClass	11
#define ASN1TAG_PaletteClass	12
#define ASN1TAG_FontClass	13
#define ASN1TAG_CursorShapeClass	14
#define ASN1TAG_BooleanVariableClass	15
#define ASN1TAG_IntegerVariableClass	16
#define ASN1TAG_OctetStringVariableClass	17
#define ASN1TAG_ObjectRefVariableClass	18
#define ASN1TAG_ContentRefVariableClass	19
#define ASN1TAG_LinkClass	20
#define ASN1TAG_StreamClass	21
#define ASN1TAG_BitmapClass	22
#define ASN1TAG_LineArtClass	23
#define ASN1TAG_DynamicLineArtClass	24
#define ASN1TAG_RectangleClass	25
#define ASN1TAG_HotspotClass	26
#define ASN1TAG_SwitchButtonClass	27
#define ASN1TAG_PushButtonClass	28
#define ASN1TAG_TextClass	29
#define ASN1TAG_EntryFieldClass	30
#define ASN1TAG_HyperTextClass	31
#define ASN1TAG_SliderClass	32
#define ASN1TAG_TokenGroupClass	33
#define ASN1TAG_ListGroupClass	34
#define ASN1TAG_OnSpawnCloseDown	35
#define ASN1TAG_OnRestart	36
#define ASN1TAG_DefaultAttributes	37
#define ASN1TAG_CharacterSet	38
#define ASN1TAG_BackgroundColour	39
#define ASN1TAG_TextContentHook	40
#define ASN1TAG_TextColour	41
#define ASN1TAG_FontBody	42
#define ASN1TAG_FontAttributes	43
#define ASN1TAG_InterchangedProgramContentHook	44
#define ASN1TAG_StreamContentHook	45
#define ASN1TAG_BitmapContentHook	46
#define ASN1TAG_LineArtContentHook	47
#define ASN1TAG_ButtonRefColour	48
#define ASN1TAG_HighlightRefColour	49
#define ASN1TAG_SliderRefColour	50
#define ASN1TAG_InputEventRegister	51
#define ASN1TAG_SceneCoordinateSystem	52
#define ASN1TAG_AspectRatio	53
#define ASN1TAG_MovingCursor	54
#define ASN1TAG_NextScenes	55
#define ASN1TAG_InitiallyActive	56
#define ASN1TAG_ContentHook	57
#define ASN1TAG_OriginalContent	58
#define ASN1TAG_Shared	59
#define ASN1TAG_ContentSize	60
#define ASN1TAG_ContentCachePriority	61
#define ASN1TAG_LinkCondition	62
#define ASN1TAG_LinkEffect	63
#define ASN1TAG_Name	64
#define ASN1TAG_InitiallyAvailable	65
#define ASN1TAG_ProgramConnectionTag	66
#define ASN1TAG_OriginalValue	67
#define ASN1TAG_ObjectReference	68
#define ASN1TAG_ContentReference	69
#define ASN1TAG_MovementTable	70
#define ASN1TAG_TokenGroupItems	71
#define ASN1TAG_NoTokenActionSlots	72
#define ASN1TAG_Positions	73
#define ASN1TAG_WrapAround	74
#define ASN1TAG_MultipleSelection	75
#define ASN1TAG_BoxSize	76
#define ASN1TAG_OriginalPosition	77
#define ASN1TAG_OriginalPaletteRef	78
#define ASN1TAG_Tiling	79
#define ASN1TAG_OriginalTransparency	80
#define ASN1TAG_BorderedBoundingBox	81
#define ASN1TAG_OriginalLineWidth	82
#define ASN1TAG_OriginalRefLineColour	84
#define ASN1TAG_OriginalRefFillColour	85
#define ASN1TAG_OriginalFont	86
#define ASN1TAG_HorizontalJustification	87
#define ASN1TAG_VerticalJustification	88
#define ASN1TAG_LineOrientation	89
#define ASN1TAG_StartCorner	90
#define ASN1TAG_TextWrapping	91
#define ASN1TAG_Multiplex	92
#define ASN1TAG_Storage	93
// TODO 94
#define ASN1TAG_AudioClass	95
#define ASN1TAG_VideoClass	96
#define ASN1TAG_RTGraphicsClass	97
#define ASN1TAG_ComponentTag	98
#define ASN1TAG_OriginalVolume	99
#define ASN1TAG_Termination	100
#define ASN1TAG_EngineResp	101
#define ASN1TAG_Orientation	102
#define ASN1TAG_MaxValue	103
#define ASN1TAG_MinValue	104
#define ASN1TAG_InitialValue	105
#define ASN1TAG_InitialPortion	106
#define ASN1TAG_StepSize	107
#define ASN1TAG_SliderStyle	108
#define ASN1TAG_InputType	109
#define ASN1TAG_CharList	110
#define ASN1TAG_ObscuredInput	111
#define ASN1TAG_MaxLength	112
#define ASN1TAG_OriginalLabel	113
#define ASN1TAG_ButtonStyle	114
#define ASN1TAG_Activate	115
#define ASN1TAG_Add	116
#define ASN1TAG_AddItem	117
#define ASN1TAG_Append	118
#define ASN1TAG_BringToFront	119
#define ASN1TAG_Call	120
#define ASN1TAG_CallActionSlot	121
#define ASN1TAG_Clear	122
#define ASN1TAG_Clone	123
#define ASN1TAG_CloseConnection	124
#define ASN1TAG_Deactivate	125
#define ASN1TAG_DelItem	126
#define ASN1TAG_Deselect	127
#define ASN1TAG_DeselectItem	128
#define ASN1TAG_Divide	129
#define ASN1TAG_DrawArc	130
#define ASN1TAG_DrawLine	131
#define ASN1TAG_DrawOval	132
#define ASN1TAG_DrawPolygon	133
#define ASN1TAG_DrawPolyline	134
#define ASN1TAG_DrawRectangle	135
#define ASN1TAG_DrawSector	136
#define ASN1TAG_Fork	137
#define ASN1TAG_GetAvailabilityStatus	138
#define ASN1TAG_GetBoxSize	139
#define ASN1TAG_GetCellItem	140
#define ASN1TAG_GetCursorPosition	141
#define ASN1TAG_GetEngineSupport	142
#define ASN1TAG_GetEntryPoint	143
#define ASN1TAG_GetFillColour	144
#define ASN1TAG_GetFirstItem	145
#define ASN1TAG_GetHighlightStatus	146
#define ASN1TAG_GetInteractionStatus	147
#define ASN1TAG_GetItemStatus	148
#define ASN1TAG_GetLabel	149
#define ASN1TAG_GetLastAnchorFired	150
#define ASN1TAG_GetLineColour	151
#define ASN1TAG_GetLineStyle	152
#define ASN1TAG_GetLineWidth	153
#define ASN1TAG_GetListItem	154
#define ASN1TAG_GetListSize	155
#define ASN1TAG_GetOverwriteMode	156
#define ASN1TAG_GetPortion	157
#define ASN1TAG_GetPosition	158
#define ASN1TAG_GetRunningStatus	159
#define ASN1TAG_GetSelectionStatus	160
#define ASN1TAG_GetSliderValue	161
#define ASN1TAG_GetTextContent	162
#define ASN1TAG_GetTextData	163
#define ASN1TAG_GetTokenPosition	164
#define ASN1TAG_GetVolume	165
#define ASN1TAG_Launch	166
#define ASN1TAG_LockScreen	167
#define ASN1TAG_Modulo	168
#define ASN1TAG_Move	169
#define ASN1TAG_MoveTo	170
#define ASN1TAG_Multiply	171
#define ASN1TAG_OpenConnection	172
#define ASN1TAG_Preload	173
#define ASN1TAG_PutBefore	174
#define ASN1TAG_PutBehind	175
#define ASN1TAG_Quit	176
#define ASN1TAG_ReadPersistent	177
#define ASN1TAG_Run	178
#define ASN1TAG_ScaleBitmap	179
#define ASN1TAG_ScaleVideo	180
#define ASN1TAG_ScrollItems	181
#define ASN1TAG_Select	182
#define ASN1TAG_SelectItem	183
#define ASN1TAG_SendEvent	184
#define ASN1TAG_SendToBack	185
#define ASN1TAG_SetBoxSize	186
#define ASN1TAG_SetCachePriority	187
#define ASN1TAG_SetCounterEndPosition	188
#define ASN1TAG_SetCounterPosition	189
#define ASN1TAG_SetCounterTrigger	190
#define ASN1TAG_SetCursorPosition	191
#define ASN1TAG_SetCursorShape	192
#define ASN1TAG_SetData	193
#define ASN1TAG_SetEntryPoint	194
#define ASN1TAG_SetFillColour	195
#define ASN1TAG_SetFirstItem	196
#define ASN1TAG_SetFontRef	197
#define ASN1TAG_SetHighlightStatus	198
#define ASN1TAG_SetInteractionStatus	199
#define ASN1TAG_SetLabel	200
#define ASN1TAG_SetLineColour	201
#define ASN1TAG_SetLineStyle	202
#define ASN1TAG_SetLineWidth	203
#define ASN1TAG_SetOverwriteMode	204
#define ASN1TAG_SetPaletteRef	205
#define ASN1TAG_SetPortion	206
#define ASN1TAG_SetPosition	207
#define ASN1TAG_SetSliderValue	208
#define ASN1TAG_SetSpeed	209
#define ASN1TAG_SetTimer	210
#define ASN1TAG_SetTransparency	211
#define ASN1TAG_SetVariable	212
#define ASN1TAG_SetVolume	213
#define ASN1TAG_Spawn	214
#define ASN1TAG_Step	215
#define ASN1TAG_Stop	216
#define ASN1TAG_StorePersistent	217
#define ASN1TAG_Subtract	218
#define ASN1TAG_TestVariable	219
#define ASN1TAG_Toggle	220
#define ASN1TAG_ToggleItem	221
#define ASN1TAG_TransitionTo	222
#define ASN1TAG_Unload	223
#define ASN1TAG_UnlockScreen	224
#define ASN1TAG_NewGenericBoolean	225
#define ASN1TAG_NewGenericInteger	226
#define ASN1TAG_NewGenericOctetstring	227
#define ASN1TAG_NewGenericObjectReference	228
#define ASN1TAG_NewGenericContentReference	229
#define ASN1TAG_NewColourIndex	230
#define ASN1TAG_NewAbsoluteColour	231
#define ASN1TAG_NewFontName	232
#define ASN1TAG_NewFontReference	233
#define ASN1TAG_NewContentSize	234
#define ASN1TAG_NewContentCachePriority	235
// TODO 236
#define ASN1TAG_SetBackgroundColour	237
#define ASN1TAG_SetCellPosition	238
#define ASN1TAG_SetInputReg	239
#define ASN1TAG_SetTextColour	240
#define ASN1TAG_SetFontAttributes	241
#define ASN1TAG_SetVideoDecodeOffset	242
#define ASN1TAG_GetVideoDecodeOffset	243
#define ASN1TAG_GetFocusPosition	244
#define ASN1TAG_SetFocusPosition	245
#define ASN1TAG_SetBitmapDecodeOffset	246
#define ASN1TAG_GetBitmapDecodeOffset	247
#define ASN1TAG_SetSliderParameters	248

#endif	/* __ASN1TAG_H__ */

