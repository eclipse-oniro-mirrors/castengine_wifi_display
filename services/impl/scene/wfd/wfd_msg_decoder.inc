/*
 * Copyright (c) 2023 Shenzhen Kaihong Digital Industry Development Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

IPC_MESSAGE_DECODE_BIND(WfdCommonRsp, WfdCommonRsp::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdSinkStartReq, WfdSinkStartReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdSinkStopReq, WfdSinkStopReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdAppendSurfaceReq, WfdAppendSurfaceReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdRemoveSurfaceReq, WfdRemoveSurfaceReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(SetMediaFormatReq, SetMediaFormatReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdPlayReq, WfdPlayReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdPauseReq, WfdPauseReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdCloseReq, WfdCloseReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(SetSceneTypeReq, SetSceneTypeReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(MuteReq, MuteReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(UnMuteReq, UnMuteReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(GetSinkConfigReq, GetSinkConfigReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(GetSinkConfigRsp, GetSinkConfigRsp::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdErrorMsg, WfdErrorMsg::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdConnectionChangedMsg, WfdConnectionChangedMsg::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdDecoderAccelerationDoneMsg, WfdDecoderAccelerationDoneMsg::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdSurfaceFailureMsg, WfdSurfaceFailureMsg::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdSourceStartDiscoveryReq, WfdSourceStartDiscoveryReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdSourceStopDiscoveryReq, WfdSourceStopDiscoveryReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdSourceDeviceFoundMsg, WfdSourceDeviceFoundMsg::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdSourceAddDeviceReq, WfdSourceAddDeviceReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdSourceRemoveDeviceReq, WfdSourceRemoveDeviceReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(DestroyScreenCaptureReq, DestroyScreenCaptureReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdGetBoundDevicesReq, WfdGetBoundDevicesReq::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdGetBoundDevicesRsp, WfdGetBoundDevicesRsp::MSG_ID)
IPC_MESSAGE_DECODE_BIND(WfdDeleteBoundDeviceReq, WfdDeleteBoundDeviceReq::MSG_ID)
