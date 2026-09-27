# source-analysis/ — Deep Source Analysis + ผลขุดสมอง OpenCode

> 2026-08-25 · วิเคราะห์โดย Sisyphus (surgical structure-scan method — explore agents timeout ทั้ง 2 รอบจึงทำเอง)

Last login: Mon Sep 28 03:37:56 on ttys000
/Users/ppbk/Desktop/เทส/ArchitecturalDiagnostic-MasterEngine.command ; exit;
ppbk@MacBook-Pro ~ % /Users/ppbk/Desktop/เทส/ArchitecturalDiagnostic-MasterEngine.command ; exit;




















==========================================================================================
    IntelReviveGPU BARE-METAL & ARCHITECTURAL ULTIMATE FORENSIC SUITE [THE MASTER ENGINE]  
    Timestamp: 2026-09-28 03:39:39 | Target: Intel Core 5 120U (Gen12)          
==========================================================================================

========================================================================
 1. SYSTEM ENVIRONMENT & BOOT-ARGS CONVENTION CHECKS
========================================================================
--- Operating System Metadata ---
ProductName:		macOS
ProductVersion:		14.8.4
BuildVersion:		23J319
Darwin MacBook-Pro.local 23.6.0 Darwin Kernel Version 23.6.0: Mon Jan 19 22:02:22 PST 2026; root:xnu-10063.141.1.710.3~1/RELEASE_X86_64 x86_64
--- Live NVRAM boot-args Parser Table ---
boot-args	-v keepsyms=1 npci=0x2000 msgbuf=1048576 alcid=13 myaccelname=IntelGraphics
--- System Kernel Boot Argument Verification (Sysctl View) ---
user.posix2_version: 200112
kern.version: Darwin Kernel Version 23.6.0: Mon Jan 19 22:02:22 PST 2026; root:xnu-10063.141.1.710.3~1/RELEASE_X86_64
kern.posix1version: 200112
kern.ipc.io_policy.uuid: 1
kern.osversion: 23J319

========================================================================
 2. BARE-METAL PCI APERTURE & IOVRAM TOPOGRAPHY
========================================================================
  |   "IOKitDiagnostics" = {"Instance allocation"=0x1c2088,"Container allocation"=0x498dab,"Pageable allocation"=0x1cb4000,"Classes"={"IONDRVFramebuffer"=0x1,"IOHIDEventServiceFastPathUserClient"=0x0,"IONaturalMemoryCursor"=0x0,"IOKitDiagnosticsClient"=0x0,"AppleUSBXHCIIsochronousRequestPool"=0x2,"AppleUSBDiagnostics"=0x0,"DspFuncBuzzKill"=0x0,"IOUSBHostHIDDevice"=0x0,"AppleHDAMikeyInternalCS4208"=0x0,"IOEventLink"=0x0,"TSNPacketPool"=0x0,"AppleUSBRequest"=0x1,"AppleASMedia1042USBXHCICommandRing"=0x0,"IOAudioClientBufferSet"=0x0,"AppleSMCPMC"=0x0,"IOUSBMassStorageDriverRequestTimer"=0x0,"IOUserBlockStorageDevice"=0x0,"IOHDACodecDevice"=0x1,"AppleHDATDM_Codec"=0x0,"IORegistryEntry"=0x77,"ApplePS2Controller"=0x1,"IOUSBMassStorageUASDriver"=0x0,"AppleHDAWorkLoop"=0x1,"IORTC"=0x1,"AppleNVMeRequestPoolTagReserve"=0x0,"IOHIDevice"=0x1,"OSAction_IOUserNetworkEthernet__TxSQDataAvailable"=0x0,"IOPCIEventSource"=0x0,"AppleHDAHardwareConfigDriver"=0x0,"DspFuncUserClient"=0x0,"AppleHDAHardwareConfigDriverLoader"=0x0,"IOMemoryCursor"=0x0,"IOPMGR"=0x0,"AppleSmartBatteryManager"=0x1,"AppleHDATDM_CS42L81"=0x0,"IOBreaker"=0x0,"AppleHDAFunctionGroup_80862807"=0x0,"AppleUSBUserHCIDevice"=0x0,"IOUSBLowLatencyCommandLegacy"=0x0,"IOTimeSyncClockManagerDaemonClient"=0x2,"AppleUSBUserHCITransferQueue"=0x0,"EFIData"=0xaf,"AppleUSBXHCIPort"=0x2,"AppleSMCControl"=0x0,"DspFunc4ChOutput"=0x0,"AppleLockdownMode"=0x1,"IOSurfaceSharedEventNotification"=0x0,"IOTimeSyncSyncDaemonClient"=0x1,"AppleKeyStoreUserClient"=0x26,"AppleHDAFunctionGroupWM8800"=0x0,"IOSMBusRequest"=0x0,"IOSkywalkPacket"=0x0,"DspFuncBeamFormer"=0x0,"IOPMServiceInterestNotifier"=0x62,"AppleACPILid"=0x1,"hv_vmx_vcpua_t"=0x0,"IOHIDResourceQueue"=0x0,"IOStorage"=0x4,"AppleRSMChannelControllerClient"=0x0,"IOSkywalkLogicalLink"=0x0,"AppleUpstreamUserClientDriver"=0x1,"MyIntelGPU"=0x1,"AppleUSBRequestPool"=0x2,"IOSerialStreamSync"=0x0,"IOHIDConsumer"=0x0,"IOSharedInterruptController"=0x1,"AppleUSBHostCompositeDevice"=0x4,"IOHIDClientData"=0x5,"IOBluetoothMemoryDescriptorRetainer"=0x0,"IOGraphicsWorkLoop"=0x2,"hv_vmx_vm_t"=0x0,"AppleHDATDMBusManagerCS4208"=0x0,"OSSerializer"=0x29,"IOTimeSyncClockManagerUserClient"=0x0,"OSCollection"=0x5,"IOPCIMessagedInterruptController"=0x1,"AppleKeyStoreTestUserClient"=0x0,"IOUserEthernetResourceUserClient"=0x0,"RealtekRTS524AController"=0x0,"IOUSBDeviceUserClientV2"=0x1,"SmbusHandler"=0x1,"AppleHDAEngineOutput"=0x2,"SMCSMBusController"=0x1,"_IOServiceNullNotifier"=0x1,"IOTimeSyncUnicastUDPv4PtPPort"=0x0,"IOUserNetworkRxCompletionQueue"=0x0,"AppleCallbackPowerSourceProvider"=0x1,"IOSkywalkMemorySegment"=0x0,"IOBluetoothHCIUserClient"=0x1,"hv_vioapic_t"=0x0,"AppleIntelPCHPMC"=0x0,"IOSDComplexBlockRequest"=0x20,"AppleASMediaUSBXHCIDevice"=0x0,"IONetworkStackUserClient"=0x1,"RealtekRTS5229Controller"=0x0,"com_apple_driver_pm_cpu_reporter"=0x1,"IOTimeSyncPortManager"=0x0,"AppleDisplay"=0x1,"_IOServiceNotifier"=0x268,"IOInterleavedMemoryDescriptor"=0x0,"AppleAPFSUserClient"=0x0,"AppleHDAFunctionGroupGT216"=0x0,"AppleUSBLegacyInterfaceUserClient"=0x0,"IOServiceStateNotificationEventSource"=0x0,"FeatureUnlock"=0x1,"AppleUSBUserHCIResources"=0x1,"AppleGraphicsDevicePolicy"=0x0,"AppleHDAFunctionGroupCS4208"=0x0,"IOSkywalkEthernetInterface"=0x0,"IOSKArena"=0x7,"DspFuncVolume_4ch"=0x0,"AppleUSBXHCILPTCommandRing"=0x0,"IOUSBHostDevice"=0x6,"IOSubMemoryDescriptor"=0x6,"IOSDHostDevice"=0x1,"AppleUSBRequestCompleter"=0x1,"IOHIDTranslationServiceClient"=0x0,"IOTimeSyncEthernetInterfaceAdapter"=0x0,"IOUserNetworkWLAN"=0x0,"IOAVBNubUserClient"=0x0,"IOServiceUserNotification"=0x208,"AppleFDEKeyStoreUserClient"=0x0,"RealtekRTS5260Controller"=0x0,"IOSDBlockStorageDevice"=0x1,"AppleUSBHostResourcesTypeCBPC"=0x0,"IOPlatformSensor"=0x0,"IOSkywalkNetworkBSDClient"=0x0,"DspFuncPreGain"=0x0,"IOTimeSyncDomainDaemonClient"=0x1,"MyIntelGPUClient"=0x0,"IOHIDReportElementQueue"=0x0,"AUAEffectUnitDictionary"=0x0,"com_apple_filesystems_apfs"=0x1,"DigitizerTransducer"=0x0,"IOSKRegionMapper"=0x15,"IOBluetoothL2CAPInformationFrameMemoryBlock"=0x0,"IOMbufMemoryCursor"=0x0,"AppleUSBUserHCITransferStructPool"=0x0,"IOSKMemoryArray"=0x0,"AppleHDAController"=0x1,"IOSDCard"=0x0,"IOCPU"=0x1,"IONetworkStack"=0x1,"OSAction_IOUserNetworkEthernet__DataAvailable"=0x0,"AppleACPIEventPoller"=0x1,"DspFuncBiquad"=0x0,"IOTimeSyncNetworkPortUserClient"=0x0,"ApplePlatformEnabler"=0x1,"IOSCSIPeripheralDeviceType07"=0x0,"IOSlaveMemory"=0x0,"ACMKeybagKernelService"=0x0,"AppleHDAFunctionGroupATI_RS710"=0x0,"ACMRestrictedModeAnalyticsKernelService"=0x1,"IOSimpleReporter"=0x1a,"AppleUSBUserHCIIsochronousTransferQueue"=0x0,"AppleUSBXHCILPTHB"=0x0,"IOModemSerialStreamSync"=0x0,"IOHDIXControllerUserClient"=0x0,"IOUSBMassStorageCBIDriverNub"=0x0,"AppleHDATDM_CS42L83"=0x0,"AppleALC"=0x1,"com_apple_AppleFSCompression_AppleFSCompressionTypeZlib"=0x1,"AppleHDATDMAmpMAX98706"=0x0,"DspFunc2Dot2Crossover"=0x0,"AppleUSBLegacyRoot"=0x1,"IOUSBInterface"=0x1,"AppleHDAPathControl"=0x6,"AGPMClient"=0x0,"AppleHDATDMAmpTAS5764L"=0x0,"AppleKeyStoreTest"=0x1,"AppleNVMeWorkLoop"=0x2,"IOSCSIPrimaryCommandsDevice"=0x1,"IOTimeSyncDomainUserClient"=0x0,"AppleSMBusControllerMCP"=0x0,"AppleUSBAudioInterruptPipe"=0x0,"AppleMCCSControlGibraltar"=0x0,"IOHIDProviderPropertyMerger"=0x0,"IOUSBDevice"=0x1,"IOGDiagnosticUserClient"=0x0,"AppleSEPControl"=0x0,"AppleHDAEngineInput"=0x1,"IOServiceNotificationDispatchSource"=0x0,"IOUSBMassStorageResourceUserClient"=0x0,"RtWlanUserClientU"=0x1,"IOSkywalkNetworkInterface"=0x0,"IOHIDKeyboard"=0x0,"IOTimeSyncEdgeTimeCaptureUserClient"=0x0,"IOTimeSyncTSNInterfaceAdapter"=0x0,"AUAConfigurationDictionary"=0x0,"IOACPIPlatformDevice"=0x13b,"IOUserService"=0x1,"_IOServiceInterestNotifier"=0x90,"AppleUSBHostRequestCompleter"=0x6,"IOBluetoothLocalUtilityEventSource"=0x0,"IOFramebufferI2CInterface"=0x0,"IOPowerConnection"=0x5e,"IOFBController"=0x1,"RealtekPCICardReaderController"=0x0,"SCSITaskUserClient"=0x0,"IOWatchDogTimer"=0x0,"CoreAnalyticsEventRatePolicy"=0x1,"IOEventSource"=0xe,"IODMACommand"=0x22,"IOUserNetworkTxCompletionQueue"=0x0,"IOMachPort"=0x561,"IOPMinformeeList"=0x92,"AppleCredentialManager"=0x1,"AppleRSMChannelController"=0x1,"AppleUSB30XHCITypeCPort"=0x0,"IntelFramebuffer"=0x1,"UVCService"=0x0,"AppleACPIPowerResource"=0x0,"IOPCIHostBridgeData"=0x1,"AppleUSBHostControllerIsochEndpoint"=0x0,"AGDPUserClient"=0x0,"IOHIDEvent"=0x0,"DspFuncNoiseCanceller"=0x1,"OSDictionary"=0x393f,"IOAGPDevice"=0x0,"IOHDACodecDeviceUserClient"=0x0,"RtWlanU"=0x1,"AppleUSBXHCIParkingCommandRing"=0x0,"IOTimeSyncServiceDaemonClient"=0x5,"AppleUSBHostBulkHIDDevice"=0x0,"AppleFDEKeyStore"=0x1,"IODisplayWrangler"=0x1,"AppleCyrus"=0x1,"IOUSBMassStorageDriverUFIDevice"=0x0,"AppleUSBIORequest"=0x73,"_IOUserServerCheckInCancellationHandler"=0x0,"IOUSBPipeV2"=0x1,"AppleUSBHostLegacyClient"=0x6,"IOUSBMassStorageResource"=0x1,"DspFuncFIRdirect"=0x0,"IOTimeSyncUnicastUDPv6PtPPort"=0x0,"AppleHDATDMSinkDevice"=0x0,"AppleLMUClient"=0x1,"AppleUIOPCIUserClient"=0x0,"Dont_Steal_Mac_OS_X"=0x1,"SEPEpoch"=0x0,"IOAudioTimeIntervalFilterIIR"=0x0,"IOSurfaceRootParavirtMapperInterface"=0x0,"IOHIDEventSource"=0x3,"GTraceBuffer"=0x4,"OSSet"=0x2b3,"IOSurfaceShared"=0x14,"AppleUSBUserHCIUserClient"=0x0,"AppleIntelPanel"=0x1,"AppleNVMeRequestPool"=0x1,"DspFuncCalibrationEQ"=0x0,"AppleUSB30XHCICardReaderPort"=0x0,"IOAudioEngineUserClient"=0x2a,"AGPMHeuristic4"=0x0,"IOPCIHostBridge"=0x1,"AppleHDAWidget_80862805"=0x0,"DspFuncChOutput"=0x0,"IOCommand"=0x61,"AppleHDAWidgetAD1984"=0x0,"IOTimeSyncTranslationMach"=0x1,"IOAudioTimerEvent"=0x0,"IOSkywalkNetworkNotificationHelper"=0x0,"DspPatchPoint"=0x0,"IOTimeSyncNanosecondSnapshotService"=0x0,"AppleTDMType00"=0x0,"AppleUSBXHCIAR"=0x0,"IOSCSIProtocolInterface"=0x2,"IOPlatformIO"=0x0,"IOAVBNub"=0x1,"AppleHDAFunctionGroupALC885"=0x0,"AppleHDAFunctionGroup"=0x1,"AppleHDAFunctionGroupExternalControl"=0x0,"IOGUIDPartitionScheme"=0x2,"AppleHDANode"=0x2,"IOUSBMassStorageUFIDriverNub"=0x0,"OSCollectionIterator"=0x21,"AppleEFIRuntime"=0x1,"IOHIDPowerSourceClient"=0x0,"IOFilterScheme"=0x0,"AppleMCCSIOController"=0x2,"AppleUSBHostResources"=0x1,"IONVMeController"=0x1,"AppleUSBAudioIsocFrameList"=0x0,"IOApplePartitionScheme"=0x0,"IOSurfaceRootUserClient"=0x14,"IOSkywalkInterface"=0x0,"IOSDBlockRequestEventSource"=0x1,"IOTimeSyncTranslationPMGR"=0x0,"IODisplay"=0x1,"TDMConfig"=0x0,"IOHIDAsyncReportQueue"=0x0,"IOSkywalkPacketQueue"=0x0,"IOUserEthernetResource"=0x1,"IOBasicOutputQueue"=0x1,"AppleS3ELabController"=0x0,"OSAction_IOUserNetworkEthernet__RxSQDataAvailable"=0x0,"AppleUSBHostControllerListElement"=0x0,"AppleHDACodec"=0x1,"AppleSEPIntelIOP"=0x0,"IOUSBControllerIsochListElement"=0x0,"AppleGPUWranglerClient"=0x4,"IOPacketQueue"=0x0,"IOFramebufferSharedUserClient"=0x1,"IOUserClient"=0x19,"IOHITabletPointer"=0x0,"IOAVBAudioLoader"=0x0,"AppleUSBAudioStreamNode"=0x0,"com_apple_driver_AppleUSBMassStorageInterfaceNub"=0x0,"AppleUSBHostControllerIsochListElement"=0x0,"RestrictEvents"=0x1,"AUAControlDictionary"=0x0,"IOHIDEventSystemUserClient"=0x1,"AppleHDAWidgetFactory"=0x0,"IOHDIXHDDrive"=0x0,"IOSurfaceMemoryPoolBunch"=0x0,"DIDeviceRequestPool"=0x0,"AGPM"=0x1,"AppleSystemPolicy"=0x1,"IOSurfaceDescriptor"=0x0,"AppleIPDormancyHandler"=0x1,"DspFunc3ChOutput"=0x0,"AppleMCCSControlModule"=0x1,"BrightnessRequestEventSource"=0x0,"ACMKernelService"=0x6,"AppleASMedia3142USBXHCIUserClient"=0x0,"APFSOSNumberAtomic"=0x1f8,"IOUSBHostPipe"=0x1,"IOPolledFilePollers"=0x1,"CoreAnalyticsTestUserClient"=0x0,"RealtekRTS5289Controller"=0x0,"IOTimeSyncDaemonClientBase"=0x4,"IOHDIXHDDriveOutKernelUserClient"=0x0,"IOUSBNotification"=0x0,"AppleHDAWidgetSTAC9220"=0x0,"AppleUSBUserHCIPipe"=0x0,"IOSkywalkNetworkKDPPoller"=0x0,"IOSkywalkRxSubmissionQueue"=0x0,"IOUSBInterfaceUserClient"=0x1,"AppleUSBHostDeviceUserClient"=0xc,"AppleHDAMikeyInternal"=0x0,"IORSMCommand"=0x0,"IOFramebufferParameterHandler"=0x1,"AppleTDMControlLUN"=0x0,"IOGatedOutputQueue"=0x0,"IOSurface"=0x39,"AppleUSBRootHubDevice"=0x1,"com_apple_filesystems_lifs"=0x1,"IOReportLegend"=0x2,"AppleHDAFunctionGroupMCP89"=0x0,"SMCBatteryManager"=0x1,"AppleUSBXHCIEndpoint"=0x12,"IOSurfaceSendRight"=0x1,"IOHIDEventQueue"=0x0,"AppleIntelUSBXHCICommandRing"=0x0,"IORSMCommandQueue"=0x0,"AppleAPICInterruptController"=0x1,"AppleSMC"=0x1,"IOSkywalkNetworkController"=0x0,"AppleSmartBatteryUserClient"=0x0,"IOTimeSyncService"=0x2,"AppleXsanDriver"=0x0,"IOCommandPool"=0x8,"IOBlockStorageDevice"=0x3,"AGPMHeuristic"=0x1,"AppleRSMChannel"=0x0,"IOPMrootDomain"=0x1,"BrightnessKeys"=0x1,"AppleEffaceableStorage"=0x0,"AUAFeatureUnitDictionary"=0x0,"AppleUSBNetworkingCommandPool"=0x0,"IOUSBDeviceUserClient"=0x1,"OSUserMetaClass"=0x7b,"IOPlatformControl"=0x0,"KextAuditUserClient"=0x1,"IOI2CInterfaceUserClient"=0x0,"AppleHDAFunctionGroupAD1984"=0x0,"DspFuncControlFreak"=0x0,"_IOOpenServiceIterator"=0x0,"IOHDIXCommandQueue"=0x0,"IOCommandGate"=0x187,"EventElementCollection"=0x0,"IOUSBMassStorageDriver"=0x1,"IOWorkGroup"=0x0,"AppleUSBNetworkingHostCommandPool"=0x0,"DspParameter"=0x10,"RealtekRTS522AController"=0x0,"IOPMRequestQueue"=0x2,"IOSkywalkRxCompletionQueue"=0x0,"AppleECSMBusController"=0x0,"IOBlockStorageDriver"=0x3,"IOTimeSyncInterfaceAdapter"=0x0,"IOWatchdogx86"=0x1,"AppleHDAWidgetALC262"=0x0,"DspFuncThermalSpeakerProtection"=0x0,"IOHIDWorkLoop"=0x1,"AUASelectorUnitDictionary"=0x0,"IOBlockStorageServices"=0x1,"AppleACPIACAdapter"=0x0,"IOUSBInterfaceIterator"=0x0,"IODTPlatformExpert"=0x1,"IOAppleLabelScheme"=0x0,"IOSlaveMemoryBuffer"=0x0,"IOMbufBigMemoryCursor"=0x0,"AppleUSBXHCIInterrupter"=0x1,"AppleUSBXHCICommandRing"=0x2,"AppleUSBXHCIPCI"=0x2,"AppleUSBXHCIIsochronousEndpoint"=0x0,"RealtekUSBSDXCSlot"=0x1,"IOBluetoothMemoryBlock"=0x0,"IOSurfaceMemoryPool"=0x0,"IOTimeSyncLocalClockPort"=0x1,"AUAUnitDictionary"=0x0,"AppleHDAControllerUserClient"=0x0,"AppleACPIPS2Nub"=0x1,"IOTimeSyncDaemonService"=0x1,"RootDomainUserClient"=0x4f,"DspFuncSplineLimiter"=0x0,"AppleAPFSContainer"=0x1,"AppleSEPDiscovery"=0x0,"DspFuncMozartCompressorDualBand"=0x0,"IONDRV"=0x1,"IOPCIDevice"=0x15,"AppleUSB20XHCIPort"=0x7,"IOBluetoothInactivityTimerEventSource"=0x0,"OSNumber"=0x92ef,"AUAOutputTerminalDictionary"=0x0,"SMCProcessor"=0x0,"OSValueObject<AsyncReportParam>"=0x0,"AppleHDAFunctionGroupExternalControl_VirtualGPO"=0x0,"AppleUSBUserHCIPort"=0x0,"IOTimeSyncTimedEdgeGeneratorUserClient"=0x0,"IOGDiagnosticGTraceClient"=0x1,"AppleUSBXHCI"=0x1,"IOTimeSyncIntervalFilter"=0x0,"IOTimeSyncNetworkPort"=0x0,"AppleUSBHostPacketFilter"=0x1,"AppleIntelCNLUSBXHCI"=0x0,"ACMFirstResponderKernelService"=0x1,"AppleHDAFunctionGroup_1002AAA0"=0x0,"IOUserNotification"=0x2,"IOTimeSyncUnicastLinkLayerPtPPort"=0x0,"IOHIDKeyboardDevice"=0x0,"AppleGPUWrangler_GPUPostStartWorkItem"=0x0,"IOHDAStream"=0x3,"AppleUSBXHCILPT"=0x0,"AppleUSBAudioStream"=0x0,"com_apple_AppleFSCompression_AppleFSCompressionTypeDataless"=0x1,"AGPMHeuristic3"=0x0,"IOSurfaceSharedEventListener"=0x0,"AppleUSBXHCITransferRing"=0x12,"SMCSuperIO"=0x0,"AppleAPFSSnapshot"=0x1,"OSDextCrash"=0x0,"IOWrappedMemoryDescriptor"=0x0,"AppleHDAFunctionGroupATI_Broadway"=0x0,"IOUSBHubDevice"=0x0,"AppleUSBHostDeviceIdler"=0x6,"IOPMWorkQueue"=0x1,"IOTimeSyncFilteredService"=0x0,"AGDCBacklightControlNub"=0x0,"IOGuardPageMemoryDescriptor"=0x0,"AppleUSB30XHCIPort"=0x3,"AppleSMBusControllerUserClient"=0x0,"IODeblocker"=0x0,"IOUserClient2022"=0x9,"AppleRTC"=0x1,"AppleUSBXHCISPT"=0x0,"ECEnabler"=0x1,"IOPlatformCtrlLoop"=0x0,"IOSharedDataQueue"=0x2,"X86PlatformShim"=0x1,"IOSyncer"=0x0,"AppleGPUWrangler_WorkItem"=0x0,"AppleUSBAudioPlugin"=0x0,"IORangeAllocator"=0x2,"IOKitRegistryCompatibility"=0x1,"IOMediaBSDClient"=0xc,"OSBoolean"=0x2,"AppleSEPCommand"=0x0,"OSAction"=0x11,"AppleUSBXHCIRequest"=0x33,"IOUserNetworkLogicalLink"=0x0,"IOTimeSyncUserFilteredServiceDaemonClient"=0x0,"OSAction_IOHIDEventService__SetUserProperties"=0x0,"IOWorkQueue"=0x1,"ApplePlatformEnablerUserClient"=0x0,"CoreAnalyticsUserClient"=0x1,"DspFuncAutoGainControl"=0x0,"AppleACPIInterruptLink"=0x0,"IOUSBMassStorageUFIDriver"=0x0,"IOHIDInterface"=0x3,"IOMapper"=0x0,"com_apple_BootCache"=0x1,"IOPCI2PCIBridge"=0x2,"IOHIDSystem"=0x1,"IOSurfaceRoot"=0x1,"IOUSBInterfaceUserClientV3"=0x2,"AppleHDA8086_9D70Controller"=0x0,"IOUSBHostStream"=0x0,"IOTimeSyncgPTPManagerDaemonClient"=0x1,"IOHIDDeviceElementContainer"=0x3,"IOUSBController"=0x1,"AppleHDAFunctionGroupATI_RS730"=0x0,"IOSDBlockRequestQueue"=0x1,"AppleHDAWidgetATI_Park"=0x0,"_IOServiceStateNotification"=0x0,"AppleBacklightParameterHandler"=0x1,"AppleACPICPU"=0x4,"IONetworkInterface"=0x1,"AppleMCCSControlFamily"=0x1,"RealtekUSBCardReaderController"=0x1,"IOTimeSyncNetworkPortDaemonClient"=0x0,"IOBluetoothACLMemoryDescriptor"=0x0,"IOHIDOOBReportDescriptor"=0x0,"AppleASMediaUSBXHCIStreamingEndpoint"=0x0,"IONetworkUserClient"=0x0,"AppleUserHIDEventService"=0x3,"IOReportUserClient"=0x3,"DspFunc2To4Splitter"=0x0,"AppleUSBXHCIPipe"=0x12,"DspFuncDRC"=0x0,"IOServiceStateNotificationDispatchSource"=0x0,"IOSkywalkLegacyEthernet"=0x0,"IOHDACodecFunction"=0x1,"RealtekSDXCSlot"=0x1,"IOUSBNub"=0x2,"AppleUSBHostMergeProperties"=0x0,"_IOConfigThread"=0x0,"AppleACPIPMC"=0x1,"AppleHDAEngine"=0x2,"IOTimeSyncClockManager"=0x1,"AppleUSBXHCIARRequest"=0x0,"AppleUSBAudioDevice"=0x0,"AppleHDAWidgetGK10X"=0x0,"OSValueObject<AsyncCommitParam>"=0x0,"IODiskImageBlockStorageDeviceOutKernel"=0x0,"AppleUSBHostController"=0x1,"IOTimeSyncFDPtPPort"=0x0,"AppleSEPXART"=0x0,"IOUSBMassStorageDriverUFIStorageServices"=0x0,"IOCPUInterruptController"=0x1,"IOHIDPowerSource"=0x1,"ApplePS2MouseDevice"=0x1,"AppleHDAFunctionGroupFactory"=0x0,"CoreAnalyticsHub"=0x1,"IOSkywalkKernelPipeBSDClient"=0x0,"IOUserServer"=0x2,"IOSDCardEventSource"=0x2,"AppleHDAFunctionGroupALC262"=0x0,"AppleIPAppenderUserClient"=0x0,"PMHaltWorker"=0x0,"AppleHDATDMDevice"=0x0,"AppleMCCSUserClient"=0x0,"IOUserNetworkPacket"=0x0,"SCSITaskUserClientIniter"=0x0,"AppleUSBXHCIFL1100"=0x0,"IOSlaveCPU"=0x0,"IOTSAEITimeSyncHandler"=0x0,"IOTimeSyncSyncUserClient"=0x0,"IOAudioTimeIntervalFilter"=0x0,"ACPI_SMC_PlatformPlugin"=0x0,"AppleUpstreamUserClient"=0x0,"AppleUSBTDMMassStorageClass"=0x0,"IOBootFramebuffer"=0x0,"OSAction_AppleSunriseHALClient_HALEventHandler"=0x0,"IOHIDResourceDeviceUserClient"=0x0,"IOInterruptController"=0x5,"DspFunc"=0x2,"AppleASMediaUSBXHCI"=0x0,"SMCPolledInterface"=0x1,"AppleUSBHostResourcesClient"=0x1,"IOEthernetInterface"=0x1,"IOSkywalkPacketPoller"=0x0,"AppleHDAFunctionGroup_80862805"=0x0,"IOTimeSyncTimeLineFilter"=0x0,"IOSDHostDriver"=0x1,"AppleANS2Controller"=0x0,"AppleUSBXHCIARIsochronousRequest"=0x0,"IOMbufLittleMemoryCursor"=0x0,"RealtekRTS5209Controller"=0x0,"AGDCPlugin"=0x1,"AppleHDATDMBusManager"=0x0,"AppleUSBHubPolicyMaker"=0x1,"IOConditionLock"=0x0,"IOGraphicsControllerWorkLoop"=0x1,"AppleUSBAudioComposite"=0x0,"AppleDiskImageDevice"=0x0,"IOUSBUserClientLegacy"=0x1,"AppleUSBHostIORequestPool"=0x12,"AppleHDAFunctionGroupGK10X"=0x0,"AppleUSBHostFrameworkClient"=0x1,"com_apple_driver_pm_pch_reporter"=0x1,"DspFuncMultiBandDRC"=0x0,"IOAccelerator"=0x0,"IOPlatformPluginLegacy"=0x0,"IOUserResources"=0x1,"AppleNVMeBuffer"=0x5,"AppleAPFSVolume"=0x6,"IOWatchdogUserClient"=0x1,"IOFenceTransaction"=0x0,"IOSkywalkStatisticsReporter"=0x0,"PMTraceWorker"=0x1,"AppleUSBXHCISparseRequest"=0x0,"NVMePMProxy"=0x0,"IOSCSIBlockCommandsDevice"=0x1,"IOAudioControl"=0x3,"ApplePS2Device"=0x2,"AppleSMBusControllerICH"=0x0,"IOSDBlockRequest"=0x1,"IONVMeBlockStorageDevice"=0x1,"AppleHDAWidgetCS4206"=0x0,"IOI2CInterface"=0x0,"AppleHDAHDMI_DPDriver"=0x0,"AppleUSBXHCIEndpointSoftRetry"=0x0,"IODTNVRAMVariables"=0x0,"IOUSBHostIOSource"=0x1,"AppleHDAFunctionGroupSTAC9220"=0x0,"IONetworkController"=0x1,"OSString"=0x9fdf,"IORSMMemoryDescriptorArray"=0x0,"AppleSunriseHALClient"=0x0,"IOUserIterator"=0x1f,"DspFunc2To6Splitter"=0x0,"AppleMCCSParameterHandler"=0x1,"IOSortableConfigurationDescriptor"=0x0,"PMSettingHandle"=0x3,"OSOrderedSet"=0xf4,"AppleMobileFileIntegrityUserClient"=0x0,"AppleUSBXHCIIsochronousRequest"=0xa,"AppleUSBXHCIStream"=0x0,"IOHIDEventDriver"=0x1,"DspFuncClientGainAdjust"=0x0,"TSNWiFiControlInterface"=0x0,"com_apple_driver_pm_flex_reporter"=0x1,"IOStateReporter"=0x1,"AppleUSBXHCIPPT"=0x0,"IOSerialBSDClient"=0x0,"IOAudioStream"=0x1,"AppleAPFSMedia"=0x1,"APFSCryptoContext"=0x240,"IONotifier"=0x4,"IOBluetoothL2CAPMemoryBlock"=0x0,"AppleHPET"=0x1,"AGPMHeuristic2"=0x1,"OSValueObject<void*>"=0x0,"AUAADC3ClassSpecificDescriptorFetcher"=0x0,"AppleUSBPipe"=0x7,"IOTimeSyncTimeLineFilterIIR128"=0x0,"IOFilterInterruptEventSource"=0x5,"IOLittleMemoryCursor"=0x0,"IOTimeSyncDaemonServiceBase"=0x1,"DspFuncGain"=0x0,"IOSDSimpleBlockRequest"=0x21,"IOUserNetworkPacketBufferPool"=0x0,"AppleSmartBatteryHFDataClient"=0x0,"IOHIDPointingDevice"=0x0,"IOSkywalkTxSubmissionQueue"=0x0,"AppleUSBXHCIWPT"=0x0,"AppleSEPIntelIOPNub"=0x0,"_IOFramebufferNotifier"=0x4,"IOBluetoothDataQueue"=0x0,"IOHDAController"=0x1,"AppleIntelSlowAdaptiveClockingManager"=0x1,"AppleUSBHostRequest"=0x2,"IODisplayAssertionUserClient"=0x0,"IOSKRegion"=0x1d,"IOMedia"=0xc,"AppleUSBAudioDictionary"=0x0,"AppleSunriseHALFunction"=0x0,"IOHistogramReporter"=0x2,"IOHIDAction"=0x0,"IOPlatformPluginFamilyPriv"=0x1,"IONVMeControllerPolledAdapter"=0x1,"IOPerfControlWorkContext"=0x71,"AppleHDAFunctionGroupExternalControlFactory"=0x0,"AppleSMBusDevice"=0x0,"IOBluetoothMemoryBlockQueue"=0x0,"EndpointSecurityDriverClient"=0x1,"OSSymbol"=0x506f,"IOSurfaceSharedEventNotificationPort"=0x0,"AppleUserHIDDevice"=0x1,"IOSMBusController"=0x1,"AppleRSMCommand"=0x0,"AppleACPIPlatformExpert"=0x1,"OSEntitlements"=0x307,"IORegistryPlane"=0x5,"AppleUSBCDCControl"=0x0,"WolfsSDXC"=0x1,"IOHIDLibUserClient"=0x0,"IOUSBMassStorageDriverNub"=0x1,"AppleIntelUSBXHCI"=0x0,"AppleUSBHostUserClient"=0x1,"AppleUSBInterfaceIterator"=0x0,"IOSlaveEndpoint"=0x0,"AppleCyrusUserClient"=0x0,"AppleHDAFunctionGroupExternalControl_GPIO"=0x0,"AppleSMBusController"=0x0,"IOInterruptEventSource"=0x25,"AppleUSBXHCIInterrupterMSI"=0x6,"IOSkywalkPacketBufferPool"=0x0,"ACPI_SMC_GPU_CtrlLoop"=0x0,"AudioAUUCDriver"=0x2,"AppleUSBUserHCI"=0x0,"OSObject"=0x65,"IOHIDTranslationEventReq"=0x0,"AppleHDAMikeyInternalCS8409"=0x0,"IOHDIXController"=0x1,"IOStateNotificationItem"=0x4,"TSNAssistedInterface"=0x0,"IOMbufNaturalMemoryCursor"=0x0,"IOWatchdog"=0x1,"AppleHDAPathSet"=0x3,"IOKDP"=0x0,"IOFence"=0x0,"AppleUSBNetworkingCommand"=0x0,"IOGraphicsDevice"=0x1,"IOResources"=0x1,"AppleNVMeRequest"=0xff,"IOUSBControllerListElement"=0x0,"AUAMixerUnitDictionary"=0x0,"DIDeviceCreatorUserClient"=0x0,"AppleHDAWidgetAD1988"=0x19,"IOGraphicsSystemWorkLoop"=0x1,"DspFuncMultiBandCompressor"=0x0,"GMetricsRecorder"=0x0,"IOSCSIPeripheralDeviceNub"=0x1,"AppleUSBXHCIARRequestPool"=0x0,"IOSurfaceDeviceCache"=0x0,"AppleUIOMemUserClient"=0x0,"AGPMController"=0x1,"AppleUSBXHCIRequestPool"=0x3,"CoreAnalyticsPipe"=0x1,"AppleGraphicsDeviceControlClient"=0x1,"IOUSBBus"=0x1,"IOServicePM"=0x92,"AppleUSBLegacyDeviceUserClient"=0x1,"IOBigMemoryCursor"=0x0,"TSNInterface"=0x0,"AppleSmartBattery"=0x1,"AppleBusControllerCS8409"=0x0,"ACMRestrictedModeKernelService"=0x1,"AppleUSBHostBusCurrentPool"=0x0,"AppleHDAWidgetMCP89"=0x0,"AppleTDMAKSServices"=0x0,"IOMemoryDescriptor"=0x2,"IOTimeSyncTimeSyncTimePort"=0x1,"AppleUIOMem"=0x1,"AppleHDAFunctionGroupATI_RS780"=0x0,"IOHIDActionQueue"=0x0,"IOUSBInterfaceUserClientV2"=0x1,"IODMAController"=0x0,"AppleUSBInterface"=0x8,"IOUSBControllerV3"=0x1,"TSNBSDStackInterface"=0x0,"AppleGraphicsControl"=0x0,"WolfsSDXCSlot"=0x1,"IOBluetoothSCOMemoryDescriptorRetainer"=0x0,"EventQueue"=0x0,"IOHIDTranslationEvent"=0x0,"AppleHDATDMBusManagerCS8409"=0x0,"AGPMEventSource"=0x1,"IOSystemStateNotification"=0x1,"SEPApNonce"=0x0,"DspFuncEQ"=0x0,"IOCatalogue"=0x1,"IOWorkLoop"=0x42,"AppleSEPEndpointService"=0x0,"AppleAPFSMediaBSDClient"=0x1,"IODataQueue"=0x1,"AppleUSBUserHCIRequestPool"=0x0,"IOHIDDevice"=0x2,"USBToolBox"=0x0,"ACMPersistentStoreKernelService"=0x1,"IOHDACodecDriver"=0x1,"IOSkywalkNetworkPacket"=0x0,"AppleHDAWidgetATI_RS730"=0x0,"IOBluetoothL2CAPChannel"=0x0,"AppleUSBHostFrameworkInterfaceClient"=0x2,"AppleHDAWidgetATI_RS780"=0x0,"AppleGCSyntheticDeviceUserClient"=0x0,"AppleGCResourceDeviceUserClient"=0x1,"IOHIDElement"=0x1,"_IOMemoryDescriptorMixedData"=0x463,"IOReportHub"=0x1,"DspFunc6ChOutput"=0x0,"IOEthernetController"=0x1,"DspFuncStereoEnhancer"=0x0,"IOSlowAdaptiveClockingDomain"=0x0,"IOTimerEventSource"=0x214,"AppleHDAFunctionGroupCS8409"=0x0,"CryptoBufferDescriptor"=0x240,"IOPCIConfigurator"=0x1,"IOSerialDriverSync"=0x0,"com_apple_filesystems_hfs_encodings"=0x1,"IOTimeSyncEthernetConcreteControllerAdapter"=0x0,"IOUserNetworkMemorySegment"=0x0,"DspFunc2WayCrossover"=0x0,"AppleSMCClient"=0x4,"AppleHDAWidgetGT216"=0x0,"AppleUSBXHCIFL1100CommandRing"=0x0,"AppleXsanScheme"=0x0,"IOServiceMessageUserNotification"=0xc1,"IOTimeSyncUserClient"=0x0,"IOReporter"=0x3,"OSObjectWrapper"=0x0,"AppleTDMAKSDriver"=0x0,"DspFuncVolume_3ch"=0x0,"AppleUSBHostBusCurrentClient"=0x9,"hv_vmx_vma_t"=0x0,"IOMultiMemoryDescriptor"=0x0,"IOPlatformPluginFamily"=0x1,"IOSCSILogicalUnitNub"=0x1,"AUAInputTerminalDictionary"=0x0,"AppleLIFSUserClient"=0x3,"AUAStreamDictionary"=0x0,"AppleHDAWidget_80862807"=0x0,"IOTimeSyncEthernetControllerAdapter"=0x0,"OSValueObject<PMAssertStruct>"=0x3,"IOSCSIHierarchicalLogicalUnit"=0x0,"IOTimeSyncDaemonServiceProcess"=0x2,"IORSMReceiveQueueEntry"=0x0,"AppleDiskImagesController"=0x1,"AppleHDATDMDeviceFactory"=0x0,"AppleSEPDeviceService"=0x0,"IOTimeSyncClockMapping"=0x2,"OSValueObject<OSKextRequestResourceCallback>"=0x0,"KDIURL"=0x0,"AGPMHeuristic1"=0x0,"IOPanicPlatform"=0x0,"IODMAEventSource"=0x0,"IOSCSIMultipathedLogicalUnit"=0x0,"IOPlatformDevice"=0x4,"AppleNVMeSMARTUserClient"=0x0,"AppleHDAWidgetCS4208"=0x0,"DspFuncSum"=0x0,"AppleUSBHostDARTDMACommand"=0x0,"IOSurfaceDescriptorComponent"=0x0,"DspFuncStereoToMono"=0x0,"AppleHDATDMAmpSSM3515"=0x0,"IOUserNetworkEthernet"=0x0,"AppleUSBHostResourcesTypeC"=0x0,"AppleUSBAudioEngine"=0x0,"AppleHDAAudioSelectorControlDP"=0x0,"IOSurfaceWiredSendRight"=0x0,"AppleUSBHostFrameworkDeviceClient"=0x0,"IOUSBMassStorageUASDriverCommand"=0x0,"IOSkywalkController"=0x0,"IOAudioControlUserClient"=0x1c,"DspBuffer"=0x6,"IOStateNotificationListener"=0x0,"IOAudioEngine"=0x1,"OSSerialize"=0x1,"ALCUserClient"=0x0,"AppleUSBDescriptorCache"=0x6,"IOSurfaceSharedEventReference"=0x0,"IOAudioDevice"=0x1,"OSKext"=0x1e4,"ApplePMC"=0x1,"AppleUSBDescriptor"=0x22,"AppleSMBIOS"=0x1,"RealtekRTS5227SeriesController"=0x0,"com_apple_driver_pm_msr_reporter"=0x1,"IOSkywalkTxCompletionQueue"=0x0,"WhateverGreen"=0x1,"SMCWatchDogTimer"=0x1,"IOTimeSyncUnicastLinkLayerEtEPort"=0x0,"AUAClockMultiplierDictionary"=0x0,"AppleGPUWrangler_GPU"=0x1,"AppleHDAFunctionGroupAD1988"=0x1,"AppleUSBHostBouncedDMACommand"=0x0,"_IOServiceJob"=0x0,"AGDPClientControl"=0x0,"RtWlanDeviceU"=0x1,"IOPMPowerStateQueue"=0x1,"NVMeFix"=0x1,"AppleEmbeddedHIDEventService"=0x0,"IOUserNetworkPacketQueue"=0x0,"OSIterator"=0x2,"hv_vcpu_t"=0x0,"IOAccelerationUserClient"=0x1,"IOBluetoothTimerEventSource"=0x0,"AppleUSBHostBusCurrentAllocator"=0xa,"IOMemoryMap"=0x227,"DspFuncDelay"=0x0,"IOAudioToggleControl"=0x6,"AppleSEPTesting"=0x0,"com_apple_driver_pmtelemetry"=0x1,"SCSITask"=0x0,"AppleSmartBatteryManagerUserClient"=0x0,"IOBluetoothL2CAPSupervisoryFrameMemoryBlock"=0x0,"MyIntelFramebuffer"=0x0,"IOHDIXHDDriveOutKernel"=0x0,"IOUserUserClient"=0x0,"AppleNVMeController"=0x0,"AppleUSBXHCIDevice"=0x6,"IOUSBMassStorageCBIDriver"=0x0,"AppleMobileFileIntegrity"=0x1,"AppleHDAEngineUserClient"=0x0,"AUAExtensionUnitDictionary"=0x0,"AGDCBacklightControl"=0x0,"AppleHDAFunctionGroupMCP79"=0x0,"X86PlatformPlugin"=0x1,"IOPCIDiagnosticsClient"=0x0,"IOHIDParamUserClient"=0x4,"IOPCIBridge"=0x2,"IORSMChannel"=0x0,"AppleASMediaUSBXHCIIsochronousRequestPool"=0x0,"AppleEffaceableStorageUserClient"=0x0,"OSData"=0x12e0,"IORegistryIterator"=0x0,"AppleUIOPCI"=0x0,"IOHDIXCommandPool"=0x0,"TransducerState"=0x0,"PMAssertionsTracker"=0x1,"IOOutputQueue"=0x1,"AppleSEPUserClient"=0x0,"IOExclaveProxy"=0x0,"EndpointSecurityExternalClient"=0x0,"AppleUSBHostDMACommand"=0x0,"AppleEmbeddedKeyboard"=0x0,"DspFuncCrossover"=0x0,"IOSCSIPeripheralDeviceType00"=0x1,"IOPMClientAck"=0x0,"IOHDIXHDDriveNub"=0x0,"AppleKeyStoreCommand"=0x2,"AppleNVMeRequestTimer"=0x1,"AppleACPIPlatformUserClient"=0x0,"IOSkywalkLegacyEthernetInterface"=0x0,"IOHIDKeyboardEventDevice"=0x2,"IOSCSITargetDevice"=0x0,"AppleBacklightDisplay"=0x0,"IOTimeSyncDaemonUserClient"=0x1,"IORootParent"=0x1,"AppleUSB20HostController"=0x0,"AppleASMedia1042USBXHCI"=0x0,"DspFuncMozartCompressor"=0x0,"com_apple_driver_pm_cstate_reporter"=0x1,"NVHDAEnabler"=0x0,"AppleUSBAudioControlInterface"=0x0,"PMSettingObject"=0x3,"IODispatchSource"=0x0,"TSNBSDInterface"=0x0,"BatteryManager"=0x1,"AppleACPIPCI"=0x1,"AppleKeyStore"=0x1,"IOUserNetworkRxSubmissionQueue"=0x0,"IOUSBHostInterfaceIterator"=0x0,"AppleUSBHostPortInterruptEventSource"=0xa,"AppleGPUWrangler_MatchNotificationWorkItem"=0x0,"IOPMinformee"=0x55,"IOUSBCommand"=0x0,"RealtekRTS5286Controller"=0x0,"IOPMPowerSource"=0x1,"AppleUSBHostDMABufferMemoryDescriptor"=0x1c,"AppleGCResource"=0x1,"IOTimeSyncReverseSync"=0x0,"hv_vmx_space_t"=0x0,"IOSlowAdaptiveClockingManager"=0x1,"EndpointSecurityDriver"=0x1,"IOTimeSyncEdgeTimeCapture"=0x0,"IOUSBLog"=0x1,"DspFuncLimiter"=0x0,"IOTimeSyncTimedEdgeGenerator"=0x0,"AppleUSBHostResourcesBusPowerClient"=0x0,"AppleSEPManager"=0x0,"AppleHDAMikeyInternalFactory"=0x0,"IOBluetoothDevice"=0x0,"AppleBusPowerController"=0x0,"DspFuncWithSharedMemory"=0x0,"IOTimeSyncgPTPManager"=0x1,"IOExtensiblePaniclog"=0x0,"TSNUserWiFiControlInterface"=0x0,"IODisplayParameterHandler"=0x3,"AppleIntelPanelA"=0x1,"IOBufferMemoryDescriptor"=0x196,"AppleUSBXHCILPTH"=0x0,"AppleACPIButton"=0x1,"AppleHDADriverUserClient"=0x0,"IOHIDEventServiceUserClient"=0x5,"AppleHDAWidgetCS8409"=0x0,"AppleSystemPolicyUserClient"=0x2,"AppleHDAWidgetALC885"=0x0,"DspFuncBeam2"=0x0,"DspFunc2Dot1Crossover"=0x0,"AppleUSBHostInterfaceUserClient"=0x0,"AppleRSMChannelServerClient"=0x0,"IOPolledInterface"=0x3,"IONDRVDevice"=0x0,"IOTimeSyncTimeLineFilter128"=0x0,"OSKextSavedMutableSegment"=0x6,"IOUserSerialUserClient"=0x0,"IOHIDElementContainer"=0x1,"AppleUSBDevice"=0x7,"IOPlatformExpertDevice"=0x1,"IOSurfaceMemoryTag"=0x39,"IOUSBHubPolicyMaker"=0x1,"IOHIPointing"=0x0,"IOBluetoothHCIController"=0x1,"IOTimeSyncFDEtEPort"=0x0,"AudioAUUC"=0x0,"AppleAPFSContainerScheme"=0x1,"IONetworkData"=0x3,"IOAudioLevelControl"=0x6,"AUAClockSourceDictionary"=0x0,"AppleUSBXHCISPTLP"=0x0,"IOECStateNotifier"=0x0,"IODTNVRAMPlatformNotifier"=0x0,"IOUSBCommandPool"=0x0,"AUAProcessingUnitDictionary"=0x0,"IORSMReceiveQueue"=0x0,"AppleAPFSVolumeBSDClient"=0x6,"AppleLMUController"=0x1,"AppleHDADriver"=0x1,"AppleImage4UserClient"=0x0,"OSAction_IOHIDEventService__SetLED"=0x0,"IOPlatformPluginDevice"=0x2,"AppleTDMAKSCommand"=0x0,"IODTNVRAMDiags"=0x0,"RealtekRTS5249SeriesController"=0x0,"IOKernelDebugger"=0x0,"IOBacklightDisplay"=0x0,"IODispatchQueue"=0x7,"CoreAnalyticsMessenger"=0x0,"AppleFIVRDriver"=0x0,"IONetworkMedium"=0x1,"IODisplayConnect"=0x1,"ACPI_SMC_PluginUserClient"=0x0,"IOSurfaceDescriptorContext"=0x0,"IOSkywalkCloneableNetworkPacket"=0x0,"IOHIDResource"=0x1,"IOUserNetworkQueueSet"=0x0,"IOTimeSyncTimeLineFilterIIR"=0x0,"AGPMHeuristic0"=0x0,"AppleHDAWidgetATI_RS710"=0x0,"AppleNVMeTranslationSMARTUserClient"=0x0,"ALCUserClientProvider"=0x0,"AppleHDAWidgetWM8800"=0x0,"AppleSATLSMARTUserClient"=0x0,"AppleUSBCDCCompositeDevice"=0x0,"AppleBSDKextStarter"=0x1,"IOTimeSyncPort"=0x1,"AppleUSBXHCISparseRequestPool"=0x0,"IOUSBRootHubDevice"=0x0,"MyIntelAccelerator"=0x0,"IOHIDPointingEventDevice"=0x0,"IOTimeSyncgPTPManagerUserClient"=0x0,"IOHIDDeviceShim"=0x1,"AppleSCSISubsystemGlobals"=0x1,"IOTimeSyncTimeOfDayPort"=0x0,"AppleHDAWidget_1002AAA0"=0x0,"OSValueObject<SMCNotification>"=0x1,"IOBluetoothObject"=0x0,"AppleUSBXHCIARIsochronousRequestPool"=0x0,"OSMetaClass"=0x0,"IOBluetoothACPIMethods"=0x1,"DspFuncVolume"=0x2,"AppleImage4"=0x1,"AppleGraphicsDeviceControl"=0x1,"IOUSBControllerV2"=0x1,"IOHIDPointing"=0x0,"IOAudioSelectorControl"=0x3,"DspFuncAudioMeter"=0x0,"IOUserSerial"=0x0,"IOUserNetworkTxSubmissionQueue"=0x0,"IOSKMapper"=0x6,"OSValueObject<__ReportResult>"=0x0,"IOSCSIProtocolServices"=0x2,"IOHIDEventDummyService"=0x0,"IOPlatformPluginThermalProfile"=0x0,"AppleEFINVRAM"=0x1,"IOKitDiagnostics"=0x1,"RealtekRTS8411SeriesController"=0x0,"IOTimeSyncDomain"=0x1,"IODTNVRAM"=0x1,"IOHIDTranslationService"=0x0,"IOHIDEventServiceQueue"=0x6,"com_apple_driver_pm_reporter"=0x8,"AppleUSBXHCITDPool"=0x2,"AppleUSBHostDMACommandPool"=0x2,"IOSurfaceMemoryRegion"=0x0,"AppleACPIEC"=0x1,"AppleRTCUserClient"=0x3,"DspFuncXTC"=0x0,"AppleUSBAudioIsocPipe"=0x0,"UIOPCIMatch"=0x0,"AppleHDAWidgetMCP79"=0x0,"hv_vm_t"=0x0,"AppleSSE"=0x1,"IOBluetoothHIDChannel"=0x0,"hv_vatpic_t"=0x0,"IOPMRequest"=0x0,"IOECTimeSyncHandler"=0x0,"AppleGCSyntheticDevice"=0x0,"IOHIDUserDevice"=0x0,"IOHIKeyboard"=0x2,"AppleGPUWrangler_BusyInterestWorkItem"=0x0,"AppleHDATDMAmpTAS5758L"=0x0,"AUAASEndpointDictionary"=0x0,"AppleHDAFunctionGroupATI_Park"=0x0,"AppleMCCSControlCello"=0x0,"AppleXsanDevice"=0x0,"IOService"=0xb3,"IOSurfaceSharedEvent"=0x0,"IORS232SerialStreamSync"=0x0,"RealtekRTS5249Controller"=0x0,"IOUserServerCheckInToken"=0x2,"IOBluetoothWorkLoop"=0x0,"AppleHDAWidget_10DE0014"=0x0,"ACMAccessoryCacheKernelService"=0x1,"IOHIDEventRepairDriver"=0x0,"AppleHDAFunctionGroup_10DE0014"=0x0,"ACPI_SMC_Idle_CtrlLoop"=0x0,"IOSKMemoryBuffer"=0x6a,"IOTimeSyncUnicastUDPv4EtEPort"=0x0,"KextAudit"=0x1,"AppleSMCFamily"=0x1,"IOFramebuffer"=0x1,"AppleIPAppender"=0x1,"AppleUSBXHCIIsochronousTransferRing"=0x0,"IOPMCompletionQueue"=0x1,"AppleNVMeRequestPoolPreTagReserve"=0x0,"AppleSEPCommandPool"=0x0,"IOTimeSyncUserFilteredService"=0x0,"DspFuncVirtualization"=0x0,"IOPlatformStateSensor"=0x0,"IOSkywalkQueueSet"=0x0,"AppleHV"=0x1,"AppleCredentialManagerUserClient"=0x4,"AppleFileSystemDriver"=0x0,"mDNSOffloadUserClient"=0x0,"IOUSBHostInterfaceLegacyIterator"=0x0,"AppleUSBHostPort"=0x1,"PassthruInterruptController"=0x0,"AppleUSBUserHCICommandQueue"=0x0,"AppleHDAWidget"=0x1,"RealtekRTS5227Controller"=0x0,"IOFDiskPartitionScheme"=0x0,"com_apple_filesystems_hfs"=0x1,"IODataQueueDispatchSource"=0x0,"AppleTDMEffaceableNORDriver"=0x0,"OSAction_IOHIDDevice__CompleteReport"=0x0,"AppleUSBXHCITR"=0x0,"AppleHDAFunctionGroupCS4206"=0x0,"AppleUSBUserHCIRequest"=0x0,"AppleUSBXHCIStreamingEndpoint"=0x0,"AppleHDACodecGeneric"=0x1,"AppleGPUWrangler_DeferredReleaseWorkItem"=0x0,"AppleSSEUserClient"=0x0,"IOUSBMassStorageInterfaceNub"=0x1,"IOHIDPowerSourceController"=0x1,"IOPMPowerSourceList"=0x0,"AppleHDAEngineOutputDP"=0x0,"com_apple_filesystems_nfs"=0x1,"OSAction_IOUserClient_KernelCompletion"=0x0,"AUAEndpointDictionary"=0x0,"mDNSHandoff"=0x0,"AppleVTDDeviceMapper"=0x0,"AppleSunriseHALDevice"=0x1,"IOTimeSyncEthernetSoftDMAInterfaceAdapter"=0x0,"AppleHDAStream"=0x5,"AppleAMDUSBXHCIPCI"=0x0,"IOUSBPipe"=0x1,"AppleACPICPUInterruptController"=0x1,"ApplePS2Keyboard"=0x1,"TSNITimeSyncHandler"=0x0,"IOAVBControllerHelper"=0x0,"AppleVTD"=0x0,"AppleSEPEndpoint"=0x0,"IOPerfControlClient"=0x3,"RealtekCardReaderController"=0x1,"ApplePS2KeyboardDevice"=0x1,"VirtualSMC"=0x1,"RealtekRTS5287Controller"=0x0,"EupDSP"=0x0,"Lilu"=0x1,"AppleBusControllerFactory"=0x0,"IOPlatformExpert"=0x1,"IOUserEthernetController"=0x0,"IOTimeSyncEthernetPort"=0x0,"IOUserSCSIPeripheralDeviceType00"=0x0,"com_apple_driver_pm_uncore_reporter"=0x1,"TSNWiFiInterface"=0x0,"AppleUSBController"=0x1,"IOTimeSyncUserFilteredServiceUserClient"=0x0,"IOPartitionScheme"=0x2,"IOTimeSyncIntervalFilter128"=0x0,"IOUserSCSIPeripheralDeviceType07"=0x0,"ACPI_SMC_CtrlLoop"=0x0,"IOSlaveFirmware"=0x0,"AMFIPass"=0x1,"IOTimeSyncRootService"=0x1,"AppleASMedia3142USBXHCI"=0x0,"RealtekPCISDXCSlot"=0x0,"IOHIDUserClient"=0x1,"IOACPIPlatformExpert"=0x1,"hv_vlapic_t"=0x0,"ACMLockdownModeKernelService"=0x1,"com_apple_driver_pm_cpu_mbox"=0x51,"IOUSBControllerIsochEndpoint"=0x0,"IOTimeSyncEthernetModernInterfaceAdapter"=0x0,"DspFuncLoudness"=0x0,"RealtekRTS525AController"=0x0,"IOSurfaceDeviceMemoryRegion"=0x0,"IOServiceCompatibility"=0x3,"IOHIDEventService"=0x3,"IOAudioTimeIntervalFilterFIR"=0x0,"AppleASMediaUSBXHCIIsochronousRequest"=0x0,"IOTimeSyncIntervalFilterIIR128"=0x0,"IOUserEthernetInterface"=0x0,"OSDextStatistics"=0x10,"IOAudioPort"=0x0,"IOInterruptDispatchSource"=0x0,"IOSkywalkPacketBuffer"=0x0,"IOHIKeyboardMapper"=0x2,"AppleUSBBusPowerClient"=0x0,"AppleSMBusPCI"=0x0,"OSAction_IOHIDEventService__CopyEvent"=0x0,"IOTimeSyncUnicastUDPv6EtEPort"=0x0,"ACMBridgeKernelService"=0x0,"IOTimeSyncIntervalFilterIIR"=0x0,"IOUSBHostInterface"=0x8,"com_apple_driver_pm_msr_limits_reporter"=0x1,"hv_vmx_vcpu_t"=0x0,"AppleGPUWrangler"=0x1,"OSArray"=0x2730,"IOGeneralMemoryDescriptor"=0x96,"IOSkywalkBSDClient"=0x0,"IOSCSICommandGate"=0x3,"IOUSBIsocCommand"=0x0,"STUCWorkLoopLock"=0x0,"IOSlaveProcessor"=0x0,"DIDeviceIOUserClient"=0x0,"IOTimeSyncClockTestUserClient"=0x0,"IOUSBWorkLoop"=0x1,"AppleCallbackPowerSource"=0x0,"IOSurfaceClient"=0x3c,"IOAudioEngineEntry"=0x0,"MyIntelAccelClient"=0x0,"com_apple_driver_pm_ltr_reporter"=0x1,"IOConfigurationDescriptorOrderedSet"=0x0,"AppleBusController"=0x0,"AppleIntelICLUSBXHCI"=0x0,"AppleGraphicsDeviceControlPlugin"=0x1,"IOCancelationWrapper"=0x0,"HibernationFixup"=0x1,"DspFuncManager"=0x3,"AppleUSB20XHCITypeCPort"=0x0,"DspFuncSplitBand"=0x0,"IOBootNDRV"=0x1,"IOHIDElementPrivate"=0x448,"AppleHDAPath"=0x3,"AppleSSEInterface"=0x1,"AGDCPluginDisplayMetrics"=0x1,"IOHITablet"=0x0,"AUAClockSelectorDictionary"=0x0,"AppleTDMBlockStorageServices"=0x0,"AppleACPIEventController"=0x1,"IOFramebufferUserClient"=0x1},"IOMalloc allocation"=0x6092821}
  +-o MacBookPro16,2  <class IOPlatformExpertDevice, id 0x100000118, registered, matched, active, busy 0 (20686 ms), retain 171>
    |   "compatible" = <4d6163426f6f6b50726f31362c3200>
    +-o AppleACPIPlatformExpert  <class AppleACPIPlatformExpert, id 0x100000119, registered, matched, active, busy 0 (5581 ms), retain 190>
    | |   "IOClass" = "AppleACPIPlatformExpert"
    | |   | +-o MyIntelGPU  <class MyIntelGPU, id 0x1000004ea, registered, matched, active, busy 0 (13 ms), retain 5>
    | |   |       "IOClass" = "MyIntelGPU"
    | |   |       "VRAM,memSize" = <0000000001000000>
    | |   |       "VRAM,totalMB" = <0010000000000000>
    | |   +-o TCPU@4  <class IOPCIDevice, id 0x100000351, registered, matched, active, busy 0 (7 ms), retain 8>
    | |   |     "compatible" = <706369313032352c3139326500706369383038362c6137316400706369636c6173732c313138303030005443505500>
    | |   |     "reg" = <00200000000000000000000000000000000000001020000200000000000000000000000000000200>
    | |   +-o PEG0@6  <class IOPCIDevice, id 0x100000352, registered, matched, active, busy 0 (474 ms), retain 11>

========================================================================
 3. IOKIT ACCELERATOR SUB-CLASSES & RENDERER ID VALIDATION
========================================================================

========================================================================
 4. PHYSICAL IRQ ROUTING VECTOR & MSI-X ARMED STATUS
========================================================================
--- GPU Low-Level IRQ Hardware Map ---
--- System-Wide Interrupt Counters & CPU Sync State ---
hw.ncpu: 4
hw.activecpu: 4

========================================================================
 5. FRAMEBUFFER PARSING, EDID OVERRIDE & PIXEL DEPTH TOPOLOGY
========================================================================
--- IORegistry Display Connection Graph ---
      "DisplayProductID" = 1815
      "DisplayVendorID" = 1970170734
--- Active Userspace plist Data Structure ---
[FOUND STRUCTURE] /Library/Displays/Contents/Resources/Overrides/DisplayVendorID-2c82/DisplayProductID-924
{
  "DisplayProductID" => 2340
  "DisplayProductName" => "KD156N2930A02"
  "DisplayVendorID" => 11394
  "IODisplayEDID" => {length = 128, bytes = 0x00ffffff ffffff00 2c827c9c 00000000 ... 39333041 3032008f }
}

========================================================================
 6. KERNEL LOG ENGINE: i915 Ring Submission, ELSP & Media Pipeline (30 Mins)
========================================================================
Password verification required for Deep Kernel Log Retrieval:
Password:
Phase 8: display-first iGPU driver — RCS/BCS rings, GEM/GGTT, framebuffer
2026-09-28 03:14:14.815762+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:617] Building translation table (FakeGen=9, RealGen=12)
2026-09-28 03:14:14.815763+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:731] Translation table built with 6 entries:
2026-09-28 03:14:14.815765+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [0] RCS0: 0x2000 → 0x2000 [skipped]
2026-09-28 03:14:14.815767+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [1] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-09-28 03:14:14.815769+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [2] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-09-28 03:14:14.815771+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [3] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-09-28 03:14:14.815773+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [4] PCH Display: 0x48000 → 0xC8000 [active]
2026-09-28 03:14:14.815774+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [5] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-09-28 03:14:14.815776+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3103] Phase 5: Translation table ready (6 entries)
2026-09-28 03:14:14.815795+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1256] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-09-28 03:14:14.932829+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3271] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-09-28 03:14:14.933176+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2194] HW Accel: Allocating RCS ring buffer...
2026-09-28 03:14:14.938302+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2217] HW Accel: pre-ring FW_ACK_GT=0x00000000 GDRST=0x00000000 CORE_STATUS=0x10800303
2026-09-28 03:14:14.938310+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2232] HW Accel: pre-ring GUC_STATUS=0x00000000 (MIA_reset=0 bootrom=0x0 ukernel=0x0 mia=0x0 auth=0)
2026-09-28 03:14:14.938315+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2235] HW Accel: pre-ring WOPCM_SIZE=0x00000000 (locked=0) DMA_OFFSET=0x00000000 (valid=0)
2026-09-28 03:14:14.938750+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2242] HW Accel: Creating RCS ring (mmio_base=0x2000)...
2026-09-28 03:14:14.938752+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=RCS mmioBase=0x2000 size=16384
2026-09-28 03:14:14.943831+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4004000 vaddr=<private> size=16384
2026-09-28 03:14:14.944045+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:14:14.949120+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4008000 (STAM+TLB)
2026-09-28 03:14:14.949126+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:14:14.949131+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:14:14.949139+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:14:14.949141+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:14:14.949271+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:14:14.960057+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x408C000 vaddr=<private> size=0x4000
2026-09-28 03:14:14.960063+0700  localhost kernel[0]: (MyIntelGPU) Ring[RCS]: ringCreate: OK — ring ready at GGTT=0x4004000 vaddr=<private>
2026-09-28 03:14:14.960065+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2256] HW Accel: RCS ring created OK (ggttOffset=0x4004000, vaddr=<private>)
2026-09-28 03:14:14.960067+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2261] HW Accel: Allocating BCS ring buffer...
2026-09-28 03:14:14.965428+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2281] HW Accel: Creating BCS ring (mmio_base=0x22000)...
2026-09-28 03:14:14.965434+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=BCS mmioBase=0x22000 size=16384
2026-09-28 03:14:14.970555+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4094000 vaddr=<private> size=16384
2026-09-28 03:14:14.970820+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:14:14.976190+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4098000 (STAM+TLB)
2026-09-28 03:14:14.976197+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:14:14.976204+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:14:14.976213+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:14:14.976219+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:14:14.976347+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:14:14.987311+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x411C000 vaddr=<private> size=0x4000
2026-09-28 03:14:14.987315+0700  localhost kernel[0]: (MyIntelGPU) Ring[BCS]: ringCreate: OK — ring ready at GGTT=0x4094000 vaddr=<private>
2026-09-28 03:14:14.987317+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2295] HW Accel: BCS ring created OK (ggttOffset=0x4094000, vaddr=<private>)
2026-09-28 03:14:14.988422+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:14:14.988445+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:14:14.988454+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x00000020
2026-09-28 03:14:14.988480+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:14:14.988493+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:14:14.988499+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008001
2026-09-28 03:14:14.988507+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8000
2026-09-28 03:14:14.988512+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988518+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988524+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988529+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988534+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988539+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988543+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988547+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988551+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988556+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988561+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2323] HW Accel: RCS init commands submitted (tail=24)
2026-09-28 03:14:14.989874+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:14:14.989883+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:14:14.989885+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x60000020
2026-09-28 03:14:14.989894+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:14:14.989896+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:14:14.989899+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008019
2026-09-28 03:14:14.989901+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8018
2026-09-28 03:14:14.989902+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989903+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989904+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989905+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989907+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989908+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989909+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989910+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989911+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989912+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989913+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2334] HW Accel: BCS init commands submitted (tail=24)
Phase 8: display-first iGPU driver — RCS/BCS rings, GEM/GGTT, framebuffer
2026-09-28 03:15:58.294143+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:617] Building translation table (FakeGen=9, RealGen=12)
2026-09-28 03:15:58.294145+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:731] Translation table built with 6 entries:
2026-09-28 03:15:58.294146+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [0] RCS0: 0x2000 → 0x2000 [skipped]
2026-09-28 03:15:58.294148+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [1] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-09-28 03:15:58.294150+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [2] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-09-28 03:15:58.294152+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [3] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-09-28 03:15:58.294153+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [4] PCH Display: 0x48000 → 0xC8000 [active]
2026-09-28 03:15:58.294154+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [5] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-09-28 03:15:58.294156+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3103] Phase 5: Translation table ready (6 entries)
2026-09-28 03:15:58.294171+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1256] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-09-28 03:15:58.407567+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3271] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-09-28 03:15:58.407918+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2194] HW Accel: Allocating RCS ring buffer...
2026-09-28 03:15:58.412744+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2217] HW Accel: pre-ring FW_ACK_GT=0x00000000 GDRST=0x00000000 CORE_STATUS=0x10800303
2026-09-28 03:15:58.412748+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2232] HW Accel: pre-ring GUC_STATUS=0x00000000 (MIA_reset=0 bootrom=0x0 ukernel=0x0 mia=0x0 auth=0)
2026-09-28 03:15:58.412751+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2235] HW Accel: pre-ring WOPCM_SIZE=0x00000000 (locked=0) DMA_OFFSET=0x00000000 (valid=0)
2026-09-28 03:15:58.413138+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2242] HW Accel: Creating RCS ring (mmio_base=0x2000)...
2026-09-28 03:15:58.413140+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=RCS mmioBase=0x2000 size=16384
2026-09-28 03:15:58.418039+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4004000 vaddr=<private> size=16384
2026-09-28 03:15:58.418268+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:15:58.423109+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4008000 (STAM+TLB)
2026-09-28 03:15:58.423114+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:15:58.423119+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:15:58.423126+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:15:58.423127+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:15:58.423259+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:15:58.433274+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x408C000 vaddr=<private> size=0x4000
2026-09-28 03:15:58.433279+0700  localhost kernel[0]: (MyIntelGPU) Ring[RCS]: ringCreate: OK — ring ready at GGTT=0x4004000 vaddr=<private>
2026-09-28 03:15:58.433281+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2256] HW Accel: RCS ring created OK (ggttOffset=0x4004000, vaddr=<private>)
2026-09-28 03:15:58.433284+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2261] HW Accel: Allocating BCS ring buffer...
2026-09-28 03:15:58.438317+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2281] HW Accel: Creating BCS ring (mmio_base=0x22000)...
2026-09-28 03:15:58.438320+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=BCS mmioBase=0x22000 size=16384
2026-09-28 03:15:58.443326+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4094000 vaddr=<private> size=16384
2026-09-28 03:15:58.443584+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:15:58.448873+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4098000 (STAM+TLB)
2026-09-28 03:15:58.448878+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:15:58.448885+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:15:58.448892+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:15:58.448895+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:15:58.449023+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:15:58.460219+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x411C000 vaddr=<private> size=0x4000
2026-09-28 03:15:58.460225+0700  localhost kernel[0]: (MyIntelGPU) Ring[BCS]: ringCreate: OK — ring ready at GGTT=0x4094000 vaddr=<private>
2026-09-28 03:15:58.460227+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2295] HW Accel: BCS ring created OK (ggttOffset=0x4094000, vaddr=<private>)
2026-09-28 03:15:58.461391+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:15:58.461404+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:15:58.461408+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x00000020
2026-09-28 03:15:58.461420+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:15:58.461423+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:15:58.461425+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008001
2026-09-28 03:15:58.461427+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8000
2026-09-28 03:15:58.461429+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461430+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461432+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461434+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461436+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461438+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461440+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461441+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461442+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461444+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461450+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2323] HW Accel: RCS init commands submitted (tail=24)
2026-09-28 03:15:58.462665+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:15:58.462675+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:15:58.462678+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x60000020
2026-09-28 03:15:58.462687+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:15:58.462690+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:15:58.462692+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008019
2026-09-28 03:15:58.462694+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8018
2026-09-28 03:15:58.462696+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462698+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462699+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462701+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462702+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462703+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462705+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462706+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462708+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462709+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462711+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2334] HW Accel: BCS init commands submitted (tail=24)
Phase 8: display-first iGPU driver — RCS/BCS rings, GEM/GGTT, framebuffer
2026-09-28 03:21:20.469328+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:617] Building translation table (FakeGen=9, RealGen=12)
2026-09-28 03:21:20.469334+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:731] Translation table built with 6 entries:
2026-09-28 03:21:20.469337+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [0] RCS0: 0x2000 → 0x2000 [skipped]
2026-09-28 03:21:20.469340+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [1] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-09-28 03:21:20.469342+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [2] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-09-28 03:21:20.469344+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [3] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-09-28 03:21:20.469345+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [4] PCH Display: 0x48000 → 0xC8000 [active]
2026-09-28 03:21:20.469347+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [5] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-09-28 03:21:20.469349+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3103] Phase 5: Translation table ready (6 entries)
2026-09-28 03:21:20.469367+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1256] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-09-28 03:21:20.584855+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3271] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-09-28 03:21:20.585239+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2194] HW Accel: Allocating RCS ring buffer...
2026-09-28 03:21:20.591019+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2217] HW Accel: pre-ring FW_ACK_GT=0x00000000 GDRST=0x00000000 CORE_STATUS=0x10800303
2026-09-28 03:21:20.591025+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2232] HW Accel: pre-ring GUC_STATUS=0x00000000 (MIA_reset=0 bootrom=0x0 ukernel=0x0 mia=0x0 auth=0)
2026-09-28 03:21:20.591027+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2235] HW Accel: pre-ring WOPCM_SIZE=0x00000000 (locked=0) DMA_OFFSET=0x00000000 (valid=0)
2026-09-28 03:21:20.591458+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2242] HW Accel: Creating RCS ring (mmio_base=0x2000)...
2026-09-28 03:21:20.591462+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=RCS mmioBase=0x2000 size=16384
2026-09-28 03:21:20.596432+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4004000 vaddr=<private> size=16384
2026-09-28 03:21:20.596660+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:21:20.601815+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4008000 (STAM+TLB)
2026-09-28 03:21:20.601823+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:21:20.601830+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:21:20.601841+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:21:20.601846+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:21:20.601981+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:21:20.614396+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x408C000 vaddr=<private> size=0x4000
2026-09-28 03:21:20.614400+0700  localhost kernel[0]: (MyIntelGPU) Ring[RCS]: ringCreate: OK — ring ready at GGTT=0x4004000 vaddr=<private>
2026-09-28 03:21:20.614402+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2256] HW Accel: RCS ring created OK (ggttOffset=0x4004000, vaddr=<private>)
2026-09-28 03:21:20.614404+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2261] HW Accel: Allocating BCS ring buffer...
2026-09-28 03:21:20.620568+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2281] HW Accel: Creating BCS ring (mmio_base=0x22000)...
2026-09-28 03:21:20.620570+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=BCS mmioBase=0x22000 size=16384
2026-09-28 03:21:20.626458+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4094000 vaddr=<private> size=16384
2026-09-28 03:21:20.626713+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:21:20.632619+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4098000 (STAM+TLB)
2026-09-28 03:21:20.632623+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:21:20.632628+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:21:20.632635+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:21:20.632637+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:21:20.632758+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:21:20.645610+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x411C000 vaddr=<private> size=0x4000
2026-09-28 03:21:20.645614+0700  localhost kernel[0]: (MyIntelGPU) Ring[BCS]: ringCreate: OK — ring ready at GGTT=0x4094000 vaddr=<private>
2026-09-28 03:21:20.645616+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2295] HW Accel: BCS ring created OK (ggttOffset=0x4094000, vaddr=<private>)
2026-09-28 03:21:20.646782+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:21:20.646796+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:21:20.646798+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x00000020
2026-09-28 03:21:20.646809+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:21:20.646811+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:21:20.646814+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008001
2026-09-28 03:21:20.646816+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8000
2026-09-28 03:21:20.646817+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646822+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646823+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646824+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646825+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646826+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646827+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646828+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646829+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646830+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646831+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2323] HW Accel: RCS init commands submitted (tail=24)
2026-09-28 03:21:20.647914+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:21:20.647924+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:21:20.647926+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x60000020
2026-09-28 03:21:20.647935+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:21:20.647937+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:21:20.647938+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008019
2026-09-28 03:21:20.647939+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8018
2026-09-28 03:21:20.647940+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647941+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647942+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647943+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647944+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647945+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647946+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647947+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647948+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647949+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647950+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2334] HW Accel: BCS init commands submitted (tail=24)
Phase 8: display-first iGPU driver — RCS/BCS rings, GEM/GGTT, framebuffer
2026-09-28 03:36:48.859504+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:617] Building translation table (FakeGen=9, RealGen=12)
2026-09-28 03:36:48.859507+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:731] Translation table built with 6 entries:
2026-09-28 03:36:48.859509+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [0] RCS0: 0x2000 → 0x2000 [skipped]
2026-09-28 03:36:48.859511+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [1] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-09-28 03:36:48.859513+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [2] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-09-28 03:36:48.859520+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [3] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-09-28 03:36:48.859522+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [4] PCH Display: 0x48000 → 0xC8000 [active]
2026-09-28 03:36:48.859523+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [5] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-09-28 03:36:48.859525+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3103] Phase 5: Translation table ready (6 entries)
2026-09-28 03:36:48.859544+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1256] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-09-28 03:36:48.975640+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3271] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-09-28 03:36:48.976004+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2194] HW Accel: Allocating RCS ring buffer...
2026-09-28 03:36:48.981259+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2217] HW Accel: pre-ring FW_ACK_GT=0x00000000 GDRST=0x00000000 CORE_STATUS=0x10800303
2026-09-28 03:36:48.981265+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2232] HW Accel: pre-ring GUC_STATUS=0x00000000 (MIA_reset=0 bootrom=0x0 ukernel=0x0 mia=0x0 auth=0)
2026-09-28 03:36:48.981268+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2235] HW Accel: pre-ring WOPCM_SIZE=0x00000000 (locked=0) DMA_OFFSET=0x00000000 (valid=0)
2026-09-28 03:36:48.981667+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2242] HW Accel: Creating RCS ring (mmio_base=0x2000)...
2026-09-28 03:36:48.981669+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=RCS mmioBase=0x2000 size=16384
2026-09-28 03:36:48.986875+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4004000 vaddr=<private> size=16384
2026-09-28 03:36:48.987088+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:36:48.992331+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4008000 (STAM+TLB)
2026-09-28 03:36:48.992337+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:36:48.992342+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:36:48.992350+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:36:48.992352+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:36:48.992506+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:36:49.003006+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x408C000 vaddr=<private> size=0x4000
2026-09-28 03:36:49.003010+0700  localhost kernel[0]: (MyIntelGPU) Ring[RCS]: ringCreate: OK — ring ready at GGTT=0x4004000 vaddr=<private>
2026-09-28 03:36:49.003012+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2256] HW Accel: RCS ring created OK (ggttOffset=0x4004000, vaddr=<private>)
2026-09-28 03:36:49.003019+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2261] HW Accel: Allocating BCS ring buffer...
2026-09-28 03:36:49.008134+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2281] HW Accel: Creating BCS ring (mmio_base=0x22000)...
2026-09-28 03:36:49.008136+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=BCS mmioBase=0x22000 size=16384
2026-09-28 03:36:49.013444+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4094000 vaddr=<private> size=16384
2026-09-28 03:36:49.013707+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:36:49.018739+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4098000 (STAM+TLB)
2026-09-28 03:36:49.018744+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:36:49.018749+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:36:49.018755+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:36:49.018757+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:36:49.018881+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:36:49.029687+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x411C000 vaddr=<private> size=0x4000
2026-09-28 03:36:49.029692+0700  localhost kernel[0]: (MyIntelGPU) Ring[BCS]: ringCreate: OK — ring ready at GGTT=0x4094000 vaddr=<private>
2026-09-28 03:36:49.029695+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2295] HW Accel: BCS ring created OK (ggttOffset=0x4094000, vaddr=<private>)
2026-09-28 03:36:49.030847+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:36:49.030857+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:36:49.030860+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x00000020
2026-09-28 03:36:49.030871+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:36:49.030874+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:36:49.030876+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008001
2026-09-28 03:36:49.030878+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8000
2026-09-28 03:36:49.030879+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030881+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030882+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030883+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030885+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030886+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030887+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030888+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030890+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030891+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030892+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2323] HW Accel: RCS init commands submitted (tail=24)
2026-09-28 03:36:49.031960+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:36:49.031968+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:36:49.031970+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x60000020
2026-09-28 03:36:49.031983+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:36:49.031986+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:36:49.031988+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008019
2026-09-28 03:36:49.031989+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8018
2026-09-28 03:36:49.031991+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031992+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031993+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031994+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031996+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031997+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031998+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031999+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.032000+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.032002+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.032003+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2334] HW Accel: BCS init commands submitted (tail=24)

========================================================================
 7. FORENSIC CRASH AUDIT: Userspace Metal Intruder & IPC Interruption
========================================================================
2026-09-28 03:12:06.369821+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(429) deny(1) file-read-xattr /Library/Extensions/MyIntelGPU.kext
2026-09-28 03:12:16.841231+0700  localhost kernel[0]: KextLog: AuxKC bundle com.pongpan-bk.MyIntelGPU marked as NOT loadable
2026-09-28 03:13:13.768418+0700  localhost kernel[0]: KextLog: AuxKC bundle com.pongpan-bk.MyIntelGPU marked as loadable
2026-09-28 03:13:13.770077+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(429) deny(1) file-read-xattr /Library/Extensions/MyIntelGPU.kext
2026-09-28 03:14:14.709878+0700  localhost kernel[0]: Driver com.apple.DriverKit-IOUserDockChannelSerial has crashed 0 time(s)
2026-09-28 03:14:14.811308+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:14:14.811555+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:14:14.811854+0700  localhost kernel[0]: (MyIntelGPU) MyIntelFB: [init:69] init() OK - 1920x1080 @ 60Hz
2026-09-28 03:14:14.811870+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [init:286] init() — OK
2026-09-28 03:14:14.811871+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU v2.0.240 | First Custom iGPU Driver for Intel Core 5 120U on macOS
https://github.com/pongpan-bk/MyIntelGPU-MacDriver-HonestBridge
2026-09-28 03:14:14.812033+0700  localhost kernel[0]: (MyIntelGPU) MyIntelFB: [free:75] free()
2026-09-28 03:14:14.812056+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2853] Phase 1: PCI setup OK
2026-09-28 03:14:14.812061+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P1
2026-09-28 03:14:14.812067+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2925] BAR0 desc: length=0x1000000
2026-09-28 03:14:14.812378+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2960] Phase 2: BAR0 MMIO mapped at 0x<private> (size=0x1000000)
2026-09-28 03:14:14.812380+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P2
2026-09-28 03:14:14.812384+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:427] PCI DeviceID = 0xA7AC, Revision = 0x04
2026-09-28 03:14:14.812385+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:481] Detected Raptor Lake (Gen12) → Faking Coffee Lake (Gen9)
2026-09-28 03:14:14.812389+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:547] GMD_ID not available — using PCI ID fallback
2026-09-28 03:14:14.812390+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2976] Phase 3: Hardware detected — Gen=12, FakeGen=9
2026-09-28 03:14:14.812391+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P3
2026-09-28 03:14:14.812393+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3002] Device memory count: 3
2026-09-28 03:14:14.812395+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3022] BAR2: config phys=0x60000000 is64=1
2026-09-28 03:14:14.812400+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=0 tag=0x82001010 phys=0x51000000 len=0x1000000
2026-09-28 03:14:14.812431+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=1 tag=0xC2001018 phys=0x60000000 len=0x10000000
2026-09-28 03:14:14.812437+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=2 tag=0x81001020 phys=0x4000 len=0x40
2026-09-28 03:14:14.815756+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3081] Phase 4: BAR2 aperture mapped at 0x<private> (size=0x10000000)
2026-09-28 03:14:14.815759+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P4
2026-09-28 03:14:14.815762+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:617] Building translation table (FakeGen=9, RealGen=12)
2026-09-28 03:14:14.815763+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:731] Translation table built with 6 entries:
2026-09-28 03:14:14.815765+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [0] RCS0: 0x2000 → 0x2000 [skipped]
2026-09-28 03:14:14.815767+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [1] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-09-28 03:14:14.815769+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [2] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-09-28 03:14:14.815771+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [3] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-09-28 03:14:14.815773+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [4] PCH Display: 0x48000 → 0xC8000 [active]
2026-09-28 03:14:14.815774+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [5] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-09-28 03:14:14.815776+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3103] Phase 5: Translation table ready (6 entries)
2026-09-28 03:14:14.815777+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5
2026-09-28 03:14:14.815779+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3138] Phase 5b: Skipping display init (gEnableDisplayFramebuffer=0 _fx_g9t=9)
2026-09-28 03:14:14.815781+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3165] Phase 5c: Creating IntelFramebuffer (no interrupts)...
2026-09-28 03:14:14.815782+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5c
2026-09-28 03:14:14.815786+0700  localhost kernel[0]: (MyIntelGPU) IntelFB: [init:57] init() — OK
2026-09-28 03:14:14.815787+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3179] Phase 5c: Initialized without Interrupts
2026-09-28 03:14:14.815788+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5c-done
2026-09-28 03:14:14.815789+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3254] Phase 5d: SKIPPED MyIntelFramebuffer (GPU-only mode; pass -myintelfb to enable display)
2026-09-28 03:14:14.815792+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1223] GGTT: GMCH_CTRL=0x02C1 ggms=3
2026-09-28 03:14:14.815795+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1256] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-09-28 03:14:14.816095+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: fGsm=<private> gttTotal=1048576 (0x100000)
2026-09-28 03:14:14.816107+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[0..7]   = 000000004C800001 000000004C801001 000000004C802001 000000004C803001 000000004C804001 000000004C805001 000000004C806001 000000004C807001
2026-09-28 03:14:14.816115+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[2048]   = 000000004D000001  PTE[4096]  = 000000004D800001
2026-09-28 03:14:14.816119+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[65536]  = 0000000000000000  PTE[last-4] = 0000000000000000
2026-09-28 03:14:14.829602+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:14:14.836581+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: non-zero in first 65536 PTEs = 16822
2026-09-28 03:14:14.841499+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: framebuffer PTE run ends at page 16384 (64 MB)
2026-09-28 03:14:14.841503+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: fb PTE run base=0x4C800000 pages=16384 — zeroing only, stolen comes from DSMBASE/GMS
2026-09-28 03:14:14.932812+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: zeroed GGTT pages [16384 .. 1048575)
2026-09-28 03:14:14.932818+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1381] VRAMPOOL: capacity=4096 MB (1048576 GTT pages) — report target 4096 MB
2026-09-28 03:14:14.932827+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1383] GGTT: OK — fGsm=<private> fGttTotal=1048576
2026-09-28 03:14:14.932829+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3271] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-09-28 03:14:14.932831+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P6
2026-09-28 03:14:14.932835+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2151] === HW Accel Init Start ===
2026-09-28 03:14:14.933163+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:14:14.933173+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [disablePowerGating:2010] PG/RC6: Disabled (MISCCPCTL=0xFFFFFFFE RC_CONTROL=0x00000000)
2026-09-28 03:14:14.933176+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2194] HW Accel: Allocating RCS ring buffer...
2026-09-28 03:14:14.938291+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x42FC27000 ggttOffset=0x4000000
2026-09-28 03:14:14.938302+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2217] HW Accel: pre-ring FW_ACK_GT=0x00000000 GDRST=0x00000000 CORE_STATUS=0x10800303
2026-09-28 03:14:14.938310+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2232] HW Accel: pre-ring GUC_STATUS=0x00000000 (MIA_reset=0 bootrom=0x0 ukernel=0x0 mia=0x0 auth=0)
2026-09-28 03:14:14.938315+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2235] HW Accel: pre-ring WOPCM_SIZE=0x00000000 (locked=0) DMA_OFFSET=0x00000000 (valid=0)
2026-09-28 03:14:14.938549+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:14:14.938559+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2063] GT Reset: begin (GDRST before=0x00000000)
2026-09-28 03:14:14.938568+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2075] GT Reset: RCS RING_RESET_CTL=0x0 (req=0 ready=0 cat=0)
2026-09-28 03:14:14.938576+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2109] GT Reset: RCS ready-to-reset acked (RESET_CTL=0x3)
2026-09-28 03:14:14.938580+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2075] GT Reset: BCS RING_RESET_CTL=0x0 (req=0 ready=0 cat=0)
2026-09-28 03:14:14.938585+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2109] GT Reset: BCS ready-to-reset acked (RESET_CTL=0x3)
2026-09-28 03:14:14.938735+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2131] GT Reset: GDRST acked (GDRST after=0x00000000)
2026-09-28 03:14:14.938743+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2139] GT Reset: RCS REQUEST_RESET cleared (RESET_CTL=0x0)
2026-09-28 03:14:14.938747+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2139] GT Reset: BCS REQUEST_RESET cleared (RESET_CTL=0x0)
2026-09-28 03:14:14.938748+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2142] GT Reset: done (ok=yes)
2026-09-28 03:14:14.938750+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2242] HW Accel: Creating RCS ring (mmio_base=0x2000)...
2026-09-28 03:14:14.938752+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=RCS mmioBase=0x2000 size=16384
2026-09-28 03:14:14.943825+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x42F540000 ggttOffset=0x4004000
2026-09-28 03:14:14.943831+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4004000 vaddr=<private> size=16384
2026-09-28 03:14:14.944039+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:14:14.944045+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:14:14.949100+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=4096 pages=1 cpuAddr=<private> physAddr=0x42CF3E000 ggttOffset=0x4008000
2026-09-28 03:14:14.949120+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4008000 (STAM+TLB)
2026-09-28 03:14:14.949126+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:14:14.949131+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:14:14.949139+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:14:14.949141+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:14:14.949271+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:14:14.954864+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=536576 pages=131 cpuAddr=<private> physAddr=0x42D4FF000 ggttOffset=0x4009000
2026-09-28 03:14:14.954895+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcAllocPPGTT: OK — 131 pages @ ggtt=0x4009000 vaddr=<private> PDP0=0x42D4FF000
2026-09-28 03:14:14.960052+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x42DCD2000 ggttOffset=0x408C000
2026-09-28 03:14:14.960057+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x408C000 vaddr=<private> size=0x4000
2026-09-28 03:14:14.960060+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcBuildContext: state@+0x1000 ggtt=0x4004000 size=0x4000 CTL=0x1801 CC=0x90009
2026-09-28 03:14:14.960063+0700  localhost kernel[0]: (MyIntelGPU) Ring[RCS]: ringCreate: OK — ring ready at GGTT=0x4004000 vaddr=<private>
2026-09-28 03:14:14.960065+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2256] HW Accel: RCS ring created OK (ggttOffset=0x4004000, vaddr=<private>)
2026-09-28 03:14:14.960067+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2261] HW Accel: Allocating BCS ring buffer...
2026-09-28 03:14:14.965416+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x42E44D000 ggttOffset=0x4090000
2026-09-28 03:14:14.965428+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2281] HW Accel: Creating BCS ring (mmio_base=0x22000)...
2026-09-28 03:14:14.965434+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=BCS mmioBase=0x22000 size=16384
2026-09-28 03:14:14.970549+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x42ED08000 ggttOffset=0x4094000
2026-09-28 03:14:14.970555+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4094000 vaddr=<private> size=16384
2026-09-28 03:14:14.970814+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:14:14.970820+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:14:14.976168+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=4096 pages=1 cpuAddr=<private> physAddr=0x42ED09000 ggttOffset=0x4098000
2026-09-28 03:14:14.976190+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4098000 (STAM+TLB)
2026-09-28 03:14:14.976197+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:14:14.976204+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:14:14.976213+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:14:14.976219+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:14:14.976347+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:14:14.982324+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=536576 pages=131 cpuAddr=<private> physAddr=0x42F58C000 ggttOffset=0x4099000
2026-09-28 03:14:14.982382+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcAllocPPGTT: OK — 131 pages @ ggtt=0x4099000 vaddr=<private> PDP0=0x42F58C000
2026-09-28 03:14:14.987308+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x42F3A4000 ggttOffset=0x411C000
2026-09-28 03:14:14.987311+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x411C000 vaddr=<private> size=0x4000
2026-09-28 03:14:14.987313+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcBuildContextXcs: state@+0x1000 ggtt=0x4094000 size=0x4000 CTL=0x1801 CC=0x90009 (52 dw)
2026-09-28 03:14:14.987315+0700  localhost kernel[0]: (MyIntelGPU) Ring[BCS]: ringCreate: OK — ring ready at GGTT=0x4094000 vaddr=<private>
2026-09-28 03:14:14.987317+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2295] HW Accel: BCS ring created OK (ggttOffset=0x4094000, vaddr=<private>)
2026-09-28 03:14:14.987319+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2310] HW Accel: Emitting RCS init commands...
2026-09-28 03:14:14.987400+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:14:14.988422+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:14:14.988445+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:14:14.988454+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x00000020
2026-09-28 03:14:14.988480+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:14:14.988493+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:14:14.988499+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008001
2026-09-28 03:14:14.988507+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8000
2026-09-28 03:14:14.988512+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988518+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988524+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988529+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988534+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988539+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988543+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988547+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988551+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988556+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.988561+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2323] HW Accel: RCS init commands submitted (tail=24)
2026-09-28 03:14:14.988569+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2327] HW Accel: Emitting BCS init commands...
2026-09-28 03:14:14.988789+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:14:14.989874+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:14:14.989883+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:14:14.989885+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x60000020
2026-09-28 03:14:14.989894+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:14:14.989896+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:14:14.989899+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008019
2026-09-28 03:14:14.989901+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8018
2026-09-28 03:14:14.989902+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989903+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989904+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989905+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989907+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989908+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989909+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989910+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989911+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989912+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:14:14.989913+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2334] HW Accel: BCS init commands submitted (tail=24)
2026-09-28 03:14:14.989915+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2339] === HW Accel Init OK (partial OK) ===
2026-09-28 03:14:14.989917+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3302] Phase 6b: HW Acceleration OK (RCS=yes BCS=yes)
2026-09-28 03:14:14.989920+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P6b
2026-09-28 03:14:14.989925+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [armEngineInterrupts:3816] armEngineInterrupts: SKIPPED (boot-arg -myintelgtirq not set and no display FB)
2026-09-28 03:14:14.989930+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: VRAM Pool report on GPU node — 4096 MB (default ON, opt-out myintelvram=0)
2026-09-28 03:14:14.989946+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P6c-accel
2026-09-28 03:14:14.989947+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3395] Phase 6c: Creating MyIntelAccelerator (mode=1)
2026-09-28 03:14:14.989968+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [start:70] createAccelID OK -> id=4096
2026-09-28 03:14:14.989974+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [start:79] started (attachMode=1 accelID=4096)
2026-09-28 03:14:14.989976+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3412] Phase 6c: MyIntelAccelerator registered
2026-09-28 03:14:14.989977+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3433] Phase 7: Kext start completed successfully
2026-09-28 03:14:14.989978+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P7-done
2026-09-28 03:14:15.175381+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
gpu 0xd7eb      accel 0x10000052e     /MyIntelGPU/MyIntelAccelerator
2026-09-28 03:14:16.865966+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.865982+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.866000+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.866006+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.866047+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.866053+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.866067+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.866072+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.866100+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.866106+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.866122+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.866127+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.866153+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.866158+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.866172+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.866176+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.866201+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.866208+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.866223+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.866228+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.971219+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.971242+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.971276+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.971281+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.975631+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.977974+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.977990+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.978022+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.978028+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.978691+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.978953+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.978968+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.978996+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.979002+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.979575+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.979767+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.979777+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.979799+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.979807+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.980370+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.980544+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.980554+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.980577+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.980582+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.981137+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.981331+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:16.981343+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:16.981367+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:16.981373+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:16.981954+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:17.032682+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:17.032719+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:17.032739+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:17.032758+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:17.032775+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:17.102447+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:17.102469+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:17.102503+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:17.102509+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 9 dummy success
2026-09-28 03:14:17.103253+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 3 dummy success (generic 0x0-0x30)
2026-09-28 03:14:17.104210+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104234+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104275+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104291+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104305+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104325+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104344+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104403+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104420+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104436+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104448+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:17.104485+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
gpu 0xd7eb      accel 0x10000052e     /MyIntelGPU/MyIntelAccelerator
2026-09-28 03:14:20.337124+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.337160+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.337224+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.337245+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.337335+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.337351+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.337399+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.337417+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.337502+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.337519+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.337566+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.337584+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.337660+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.337678+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.337728+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.337767+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.337842+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.337868+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.337915+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.337932+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.347606+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.347615+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.347632+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.347635+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.347985+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.348071+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.348077+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.348095+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.348102+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.348461+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.348549+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.348554+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.348565+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.348569+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.348865+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.348940+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.348944+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.348954+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.348957+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.349251+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.349324+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.349327+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.349337+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.349339+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.349628+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.349699+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:20.349702+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:20.349711+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:20.349714+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.349998+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.392102+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.411914+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.431681+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.451423+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:20.471164+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:21.493152+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:21.493175+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:21.493210+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:21.493218+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 9 dummy success
2026-09-28 03:14:21.493241+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 3 dummy success (generic 0x0-0x30)
2026-09-28 03:14:21.497610+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497639+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497668+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497691+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497709+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497748+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497769+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497786+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497804+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497825+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497846+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
2026-09-28 03:14:21.497981+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [clientClose:278] AccelClient clientClose
gpu 0xd7eb      accel 0x10000052e     /MyIntelGPU/MyIntelAccelerator
2026-09-28 03:14:30.695226+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.695235+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.695250+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.695254+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.695275+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.695279+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.695291+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.695295+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.695313+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.695317+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.695328+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.695332+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.695350+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.695354+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.695367+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.695371+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.695389+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.695393+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.695404+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.695408+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.700154+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.700165+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.700185+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.700190+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.700724+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.700834+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.700840+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.700860+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.700864+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.701491+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.701643+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.701651+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.701670+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.701676+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.702163+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.702268+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.702274+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.702290+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.702294+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.702783+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.702882+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.702887+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.702901+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.702905+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.703392+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.703488+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:30.703493+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:30.703507+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:14:30.703511+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.703981+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.729906+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.729941+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.729958+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.729974+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:30.729990+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:322] AccelClient selector 17 dummy success (generic 0x0-0x30)
2026-09-28 03:14:32.226475+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:109] newUserClient: PROBE TRIGGERED! type=0x0 from task=<private>
2026-09-28 03:14:32.226496+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [newUserClient:145] newUserClient: EXPERIMENTAL PASS-THROUGH FOR TYPE 0x0 (WindowServer Soft-Lock Bypass)
2026-09-28 03:14:32.226532+0700  localhost kernel[0]: (MyIntelGPU) MyIntelAccelerator: [externalMethod:308] AccelClient selector 7 dummy success
2026-09-28 03:15:58.283956+0700  localhost kernel[0]: Driver com.apple.DriverKit-IOUserDockChannelSerial has crashed 0 time(s)
2026-09-28 03:15:58.290870+0700  localhost kernel[0]: (MyIntelGPU) MyIntelFB: [init:69] init() OK - 1920x1080 @ 60Hz
2026-09-28 03:15:58.290936+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [init:286] init() — OK
2026-09-28 03:15:58.290939+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU v2.0.240 | First Custom iGPU Driver for Intel Core 5 120U on macOS
https://github.com/pongpan-bk/MyIntelGPU-MacDriver-HonestBridge
2026-09-28 03:15:58.291008+0700  localhost kernel[0]: (MyIntelGPU) MyIntelFB: [free:75] free()
2026-09-28 03:15:58.291020+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2853] Phase 1: PCI setup OK
2026-09-28 03:15:58.291022+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P1
2026-09-28 03:15:58.291027+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2925] BAR0 desc: length=0x1000000
2026-09-28 03:15:58.291246+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2960] Phase 2: BAR0 MMIO mapped at 0x<private> (size=0x1000000)
2026-09-28 03:15:58.291252+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P2
2026-09-28 03:15:58.291254+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:427] PCI DeviceID = 0xA7AC, Revision = 0x04
2026-09-28 03:15:58.291256+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:481] Detected Raptor Lake (Gen12) → Faking Coffee Lake (Gen9)
2026-09-28 03:15:58.291259+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:547] GMD_ID not available — using PCI ID fallback
2026-09-28 03:15:58.291260+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2976] Phase 3: Hardware detected — Gen=12, FakeGen=9
2026-09-28 03:15:58.291261+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P3
2026-09-28 03:15:58.291262+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3002] Device memory count: 3
2026-09-28 03:15:58.291265+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3022] BAR2: config phys=0x60000000 is64=1
2026-09-28 03:15:58.291266+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=0 tag=0x82001010 phys=0x51000000 len=0x1000000
2026-09-28 03:15:58.291276+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=1 tag=0xC2001018 phys=0x60000000 len=0x10000000
2026-09-28 03:15:58.291278+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=2 tag=0x81001020 phys=0x4000 len=0x40
2026-09-28 03:15:58.294140+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3081] Phase 4: BAR2 aperture mapped at 0x<private> (size=0x10000000)
2026-09-28 03:15:58.294142+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P4
2026-09-28 03:15:58.294143+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:617] Building translation table (FakeGen=9, RealGen=12)
2026-09-28 03:15:58.294145+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:731] Translation table built with 6 entries:
2026-09-28 03:15:58.294146+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [0] RCS0: 0x2000 → 0x2000 [skipped]
2026-09-28 03:15:58.294148+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [1] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-09-28 03:15:58.294150+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [2] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-09-28 03:15:58.294152+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [3] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-09-28 03:15:58.294153+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [4] PCH Display: 0x48000 → 0xC8000 [active]
2026-09-28 03:15:58.294154+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [5] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-09-28 03:15:58.294156+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3103] Phase 5: Translation table ready (6 entries)
2026-09-28 03:15:58.294157+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5
2026-09-28 03:15:58.294158+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3138] Phase 5b: Skipping display init (gEnableDisplayFramebuffer=0 _fx_g9t=9)
2026-09-28 03:15:58.294160+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3165] Phase 5c: Creating IntelFramebuffer (no interrupts)...
2026-09-28 03:15:58.294161+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5c
2026-09-28 03:15:58.294163+0700  localhost kernel[0]: (MyIntelGPU) IntelFB: [init:57] init() — OK
2026-09-28 03:15:58.294164+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3179] Phase 5c: Initialized without Interrupts
2026-09-28 03:15:58.294165+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5c-done
2026-09-28 03:15:58.294166+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3254] Phase 5d: SKIPPED MyIntelFramebuffer (GPU-only mode; pass -myintelfb to enable display)
2026-09-28 03:15:58.294169+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1223] GGTT: GMCH_CTRL=0x02C1 ggms=3
2026-09-28 03:15:58.294171+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1256] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-09-28 03:15:58.294300+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: fGsm=<private> gttTotal=1048576 (0x100000)
2026-09-28 03:15:58.294306+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[0..7]   = 000000004C800001 000000004C801001 000000004C802001 000000004C803001 000000004C804001 000000004C805001 000000004C806001 000000004C807001
2026-09-28 03:15:58.294310+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[2048]   = 000000004D000001  PTE[4096]  = 000000004D800001
2026-09-28 03:15:58.294312+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[65536]  = A1F9A1F966386638  PTE[last-4] = 51F251F2178D178D
2026-09-28 03:15:58.313718+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: non-zero in first 65536 PTEs = 65536
2026-09-28 03:15:58.318546+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: framebuffer PTE run ends at page 16384 (64 MB)
2026-09-28 03:15:58.318549+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: fb PTE run base=0x4C800000 pages=16384 — zeroing only, stolen comes from DSMBASE/GMS
2026-09-28 03:15:58.320933+0700  localhost kernel[0]: Driver com.apple.DriverKit.AppleUserECM has crashed 0 time(s)
2026-09-28 03:15:58.342608+0700  localhost kernel[0]: Driver com.apple.DriverKit.AppleUserECM has crashed 0 time(s)
2026-09-28 03:15:58.407551+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: zeroed GGTT pages [16384 .. 1048575)
2026-09-28 03:15:58.407563+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1381] VRAMPOOL: capacity=4096 MB (1048576 GTT pages) — report target 4096 MB
2026-09-28 03:15:58.407565+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1383] GGTT: OK — fGsm=<private> fGttTotal=1048576
2026-09-28 03:15:58.407567+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3271] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-09-28 03:15:58.407568+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P6
2026-09-28 03:15:58.407574+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2151] === HW Accel Init Start ===
2026-09-28 03:15:58.407908+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:15:58.407916+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [disablePowerGating:2010] PG/RC6: Disabled (MISCCPCTL=0xFFFFFFFE RC_CONTROL=0x00000000)
2026-09-28 03:15:58.407918+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2194] HW Accel: Allocating RCS ring buffer...
2026-09-28 03:15:58.412737+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x437471000 ggttOffset=0x4000000
2026-09-28 03:15:58.412744+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2217] HW Accel: pre-ring FW_ACK_GT=0x00000000 GDRST=0x00000000 CORE_STATUS=0x10800303
2026-09-28 03:15:58.412748+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2232] HW Accel: pre-ring GUC_STATUS=0x00000000 (MIA_reset=0 bootrom=0x0 ukernel=0x0 mia=0x0 auth=0)
2026-09-28 03:15:58.412751+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2235] HW Accel: pre-ring WOPCM_SIZE=0x00000000 (locked=0) DMA_OFFSET=0x00000000 (valid=0)
2026-09-28 03:15:58.412968+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:15:58.412971+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2063] GT Reset: begin (GDRST before=0x00000000)
2026-09-28 03:15:58.412974+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2075] GT Reset: RCS RING_RESET_CTL=0x0 (req=0 ready=0 cat=0)
2026-09-28 03:15:58.412980+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2109] GT Reset: RCS ready-to-reset acked (RESET_CTL=0x3)
2026-09-28 03:15:58.412983+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2075] GT Reset: BCS RING_RESET_CTL=0x0 (req=0 ready=0 cat=0)
2026-09-28 03:15:58.412987+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2109] GT Reset: BCS ready-to-reset acked (RESET_CTL=0x3)
2026-09-28 03:15:58.413128+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2131] GT Reset: GDRST acked (GDRST after=0x00000000)
2026-09-28 03:15:58.413132+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2139] GT Reset: RCS REQUEST_RESET cleared (RESET_CTL=0x0)
2026-09-28 03:15:58.413135+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2139] GT Reset: BCS REQUEST_RESET cleared (RESET_CTL=0x0)
2026-09-28 03:15:58.413137+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2142] GT Reset: done (ok=yes)
2026-09-28 03:15:58.413138+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2242] HW Accel: Creating RCS ring (mmio_base=0x2000)...
2026-09-28 03:15:58.413140+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=RCS mmioBase=0x2000 size=16384
2026-09-28 03:15:58.418033+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x438A0C000 ggttOffset=0x4004000
2026-09-28 03:15:58.418039+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4004000 vaddr=<private> size=16384
2026-09-28 03:15:58.418263+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:15:58.418268+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:15:58.423090+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=4096 pages=1 cpuAddr=<private> physAddr=0x438A0D000 ggttOffset=0x4008000
2026-09-28 03:15:58.423109+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4008000 (STAM+TLB)
2026-09-28 03:15:58.423114+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:15:58.423119+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:15:58.423126+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:15:58.423127+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:15:58.423259+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:15:58.428340+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=536576 pages=131 cpuAddr=<private> physAddr=0x436F90000 ggttOffset=0x4009000
2026-09-28 03:15:58.428356+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcAllocPPGTT: OK — 131 pages @ ggtt=0x4009000 vaddr=<private> PDP0=0x436F90000
2026-09-28 03:15:58.433269+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x436614000 ggttOffset=0x408C000
2026-09-28 03:15:58.433274+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x408C000 vaddr=<private> size=0x4000
2026-09-28 03:15:58.433276+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcBuildContext: state@+0x1000 ggtt=0x4004000 size=0x4000 CTL=0x1801 CC=0x90009
2026-09-28 03:15:58.433279+0700  localhost kernel[0]: (MyIntelGPU) Ring[RCS]: ringCreate: OK — ring ready at GGTT=0x4004000 vaddr=<private>
2026-09-28 03:15:58.433281+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2256] HW Accel: RCS ring created OK (ggttOffset=0x4004000, vaddr=<private>)
2026-09-28 03:15:58.433284+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2261] HW Accel: Allocating BCS ring buffer...
2026-09-28 03:15:58.436458+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:15:58.436542+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:15:58.438310+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x437B07000 ggttOffset=0x4090000
2026-09-28 03:15:58.438317+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2281] HW Accel: Creating BCS ring (mmio_base=0x22000)...
2026-09-28 03:15:58.438320+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=BCS mmioBase=0x22000 size=16384
2026-09-28 03:15:58.443322+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x436E0B000 ggttOffset=0x4094000
2026-09-28 03:15:58.443326+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4094000 vaddr=<private> size=16384
2026-09-28 03:15:58.443580+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:15:58.443584+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:15:58.444051+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:15:58.448852+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=4096 pages=1 cpuAddr=<private> physAddr=0x4370F0000 ggttOffset=0x4098000
2026-09-28 03:15:58.448873+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4098000 (STAM+TLB)
2026-09-28 03:15:58.448878+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:15:58.448885+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:15:58.448892+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:15:58.448895+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:15:58.449023+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:15:58.454889+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=536576 pages=131 cpuAddr=<private> physAddr=0x435696000 ggttOffset=0x4099000
2026-09-28 03:15:58.454919+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcAllocPPGTT: OK — 131 pages @ ggtt=0x4099000 vaddr=<private> PDP0=0x435696000
2026-09-28 03:15:58.460213+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x438E1A000 ggttOffset=0x411C000
2026-09-28 03:15:58.460219+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x411C000 vaddr=<private> size=0x4000
2026-09-28 03:15:58.460221+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcBuildContextXcs: state@+0x1000 ggtt=0x4094000 size=0x4000 CTL=0x1801 CC=0x90009 (52 dw)
2026-09-28 03:15:58.460225+0700  localhost kernel[0]: (MyIntelGPU) Ring[BCS]: ringCreate: OK — ring ready at GGTT=0x4094000 vaddr=<private>
2026-09-28 03:15:58.460227+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2295] HW Accel: BCS ring created OK (ggttOffset=0x4094000, vaddr=<private>)
2026-09-28 03:15:58.460234+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2310] HW Accel: Emitting RCS init commands...
2026-09-28 03:15:58.460311+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:15:58.461391+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:15:58.461404+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:15:58.461408+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x00000020
2026-09-28 03:15:58.461420+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:15:58.461423+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:15:58.461425+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008001
2026-09-28 03:15:58.461427+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8000
2026-09-28 03:15:58.461429+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461430+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461432+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461434+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461436+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461438+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461440+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461441+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461442+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461444+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.461450+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2323] HW Accel: RCS init commands submitted (tail=24)
2026-09-28 03:15:58.461453+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2327] HW Accel: Emitting BCS init commands...
2026-09-28 03:15:58.461532+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:15:58.462665+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:15:58.462675+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:15:58.462678+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x60000020
2026-09-28 03:15:58.462687+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:15:58.462690+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:15:58.462692+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008019
2026-09-28 03:15:58.462694+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8018
2026-09-28 03:15:58.462696+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462698+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462699+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462701+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462702+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462703+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462705+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462706+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462708+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462709+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:15:58.462711+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2334] HW Accel: BCS init commands submitted (tail=24)
2026-09-28 03:15:58.462713+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2339] === HW Accel Init OK (partial OK) ===
2026-09-28 03:15:58.462716+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3302] Phase 6b: HW Acceleration OK (RCS=yes BCS=yes)
2026-09-28 03:15:58.462718+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P6b
2026-09-28 03:15:58.462724+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [armEngineInterrupts:3816] armEngineInterrupts: SKIPPED (boot-arg -myintelgtirq not set and no display FB)
2026-09-28 03:15:58.462731+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: VRAM Pool report on GPU node — 4096 MB (default ON, opt-out myintelvram=0)
2026-09-28 03:15:58.462752+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3433] Phase 7: Kext start completed successfully
2026-09-28 03:15:58.462753+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P7-done
2026-09-28 03:15:58.610146+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:16:18.939655+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:16:18.940681+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:16:20.243668+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Library/Extensions/MyIntelGPU.kext
2026-09-28 03:18:14.979435+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:18:14.984909+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:21:20.339491+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:21:20.339608+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:21:20.340139+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:21:20.427451+0700  localhost kernel[0]: Driver com.apple.DriverKit-IOUserDockChannelSerial has crashed 0 time(s)
2026-09-28 03:21:20.465402+0700  localhost kernel[0]: (MyIntelGPU) MyIntelFB: [init:69] init() OK - 1920x1080 @ 60Hz
2026-09-28 03:21:20.465417+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [init:286] init() — OK
2026-09-28 03:21:20.465419+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU v2.0.240 | First Custom iGPU Driver for Intel Core 5 120U on macOS
https://github.com/pongpan-bk/MyIntelGPU-MacDriver-HonestBridge
2026-09-28 03:21:20.465504+0700  localhost kernel[0]: (MyIntelGPU) MyIntelFB: [free:75] free()
2026-09-28 03:21:20.465516+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2853] Phase 1: PCI setup OK
2026-09-28 03:21:20.465518+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P1
2026-09-28 03:21:20.465521+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2925] BAR0 desc: length=0x1000000
2026-09-28 03:21:20.465818+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2960] Phase 2: BAR0 MMIO mapped at 0x<private> (size=0x1000000)
2026-09-28 03:21:20.465821+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P2
2026-09-28 03:21:20.465824+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:427] PCI DeviceID = 0xA7AC, Revision = 0x04
2026-09-28 03:21:20.465826+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:481] Detected Raptor Lake (Gen12) → Faking Coffee Lake (Gen9)
2026-09-28 03:21:20.465830+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:547] GMD_ID not available — using PCI ID fallback
2026-09-28 03:21:20.465831+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2976] Phase 3: Hardware detected — Gen=12, FakeGen=9
2026-09-28 03:21:20.465833+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P3
2026-09-28 03:21:20.465835+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3002] Device memory count: 3
2026-09-28 03:21:20.465837+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3022] BAR2: config phys=0x60000000 is64=1
2026-09-28 03:21:20.465839+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=0 tag=0x82001010 phys=0x51000000 len=0x1000000
2026-09-28 03:21:20.465841+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=1 tag=0xC2001018 phys=0x60000000 len=0x10000000
2026-09-28 03:21:20.465843+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=2 tag=0x81001020 phys=0x4000 len=0x40
2026-09-28 03:21:20.469324+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3081] Phase 4: BAR2 aperture mapped at 0x<private> (size=0x10000000)
2026-09-28 03:21:20.469326+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P4
2026-09-28 03:21:20.469328+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:617] Building translation table (FakeGen=9, RealGen=12)
2026-09-28 03:21:20.469334+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:731] Translation table built with 6 entries:
2026-09-28 03:21:20.469337+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [0] RCS0: 0x2000 → 0x2000 [skipped]
2026-09-28 03:21:20.469340+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [1] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-09-28 03:21:20.469342+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [2] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-09-28 03:21:20.469344+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [3] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-09-28 03:21:20.469345+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [4] PCH Display: 0x48000 → 0xC8000 [active]
2026-09-28 03:21:20.469347+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [5] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-09-28 03:21:20.469349+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3103] Phase 5: Translation table ready (6 entries)
2026-09-28 03:21:20.469350+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5
2026-09-28 03:21:20.469352+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3138] Phase 5b: Skipping display init (gEnableDisplayFramebuffer=0 _fx_g9t=9)
2026-09-28 03:21:20.469354+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3165] Phase 5c: Creating IntelFramebuffer (no interrupts)...
2026-09-28 03:21:20.469355+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5c
2026-09-28 03:21:20.469358+0700  localhost kernel[0]: (MyIntelGPU) IntelFB: [init:57] init() — OK
2026-09-28 03:21:20.469359+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3179] Phase 5c: Initialized without Interrupts
2026-09-28 03:21:20.469360+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5c-done
2026-09-28 03:21:20.469362+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3254] Phase 5d: SKIPPED MyIntelFramebuffer (GPU-only mode; pass -myintelfb to enable display)
2026-09-28 03:21:20.469364+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1223] GGTT: GMCH_CTRL=0x02C1 ggms=3
2026-09-28 03:21:20.469367+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1256] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-09-28 03:21:20.469490+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: fGsm=<private> gttTotal=1048576 (0x100000)
2026-09-28 03:21:20.469494+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[0..7]   = 000000004C800001 000000004C801001 000000004C802001 000000004C803001 000000004C804001 000000004C805001 000000004C806001 000000004C807001
2026-09-28 03:21:20.469498+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[2048]   = 000000004D000001  PTE[4096]  = 000000004D800001
2026-09-28 03:21:20.469500+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[65536]  = 0000000000000000  PTE[last-4] = 0000000000000000
2026-09-28 03:21:20.489077+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: non-zero in first 65536 PTEs = 16672
2026-09-28 03:21:20.493996+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: framebuffer PTE run ends at page 16384 (64 MB)
2026-09-28 03:21:20.493997+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: fb PTE run base=0x4C800000 pages=16384 — zeroing only, stolen comes from DSMBASE/GMS
2026-09-28 03:21:20.584840+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: zeroed GGTT pages [16384 .. 1048575)
2026-09-28 03:21:20.584850+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1381] VRAMPOOL: capacity=4096 MB (1048576 GTT pages) — report target 4096 MB
2026-09-28 03:21:20.584853+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1383] GGTT: OK — fGsm=<private> fGttTotal=1048576
2026-09-28 03:21:20.584855+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3271] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-09-28 03:21:20.584857+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P6
2026-09-28 03:21:20.584874+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2151] === HW Accel Init Start ===
2026-09-28 03:21:20.585216+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:21:20.585235+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [disablePowerGating:2010] PG/RC6: Disabled (MISCCPCTL=0xFFFFFFFE RC_CONTROL=0x00000000)
2026-09-28 03:21:20.585239+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2194] HW Accel: Allocating RCS ring buffer...
2026-09-28 03:21:20.591011+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x43B9BB000 ggttOffset=0x4000000
2026-09-28 03:21:20.591019+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2217] HW Accel: pre-ring FW_ACK_GT=0x00000000 GDRST=0x00000000 CORE_STATUS=0x10800303
2026-09-28 03:21:20.591025+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2232] HW Accel: pre-ring GUC_STATUS=0x00000000 (MIA_reset=0 bootrom=0x0 ukernel=0x0 mia=0x0 auth=0)
2026-09-28 03:21:20.591027+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2235] HW Accel: pre-ring WOPCM_SIZE=0x00000000 (locked=0) DMA_OFFSET=0x00000000 (valid=0)
2026-09-28 03:21:20.591245+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:21:20.591255+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2063] GT Reset: begin (GDRST before=0x00000000)
2026-09-28 03:21:20.591261+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2075] GT Reset: RCS RING_RESET_CTL=0x0 (req=0 ready=0 cat=0)
2026-09-28 03:21:20.591272+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2109] GT Reset: RCS ready-to-reset acked (RESET_CTL=0x3)
2026-09-28 03:21:20.591278+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2075] GT Reset: BCS RING_RESET_CTL=0x0 (req=0 ready=0 cat=0)
2026-09-28 03:21:20.591286+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2109] GT Reset: BCS ready-to-reset acked (RESET_CTL=0x3)
2026-09-28 03:21:20.591439+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2131] GT Reset: GDRST acked (GDRST after=0x00000000)
2026-09-28 03:21:20.591446+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2139] GT Reset: RCS REQUEST_RESET cleared (RESET_CTL=0x0)
2026-09-28 03:21:20.591452+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2139] GT Reset: BCS REQUEST_RESET cleared (RESET_CTL=0x0)
2026-09-28 03:21:20.591455+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2142] GT Reset: done (ok=yes)
2026-09-28 03:21:20.591458+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2242] HW Accel: Creating RCS ring (mmio_base=0x2000)...
2026-09-28 03:21:20.591462+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=RCS mmioBase=0x2000 size=16384
2026-09-28 03:21:20.596425+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x432EC5000 ggttOffset=0x4004000
2026-09-28 03:21:20.596432+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4004000 vaddr=<private> size=16384
2026-09-28 03:21:20.596653+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:21:20.596660+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:21:20.601794+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=4096 pages=1 cpuAddr=<private> physAddr=0x432EC6000 ggttOffset=0x4008000
2026-09-28 03:21:20.601815+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4008000 (STAM+TLB)
2026-09-28 03:21:20.601823+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:21:20.601830+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:21:20.601841+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:21:20.601846+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:21:20.601981+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:21:20.608410+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=536576 pages=131 cpuAddr=<private> physAddr=0x434FC9000 ggttOffset=0x4009000
2026-09-28 03:21:20.608483+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcAllocPPGTT: OK — 131 pages @ ggtt=0x4009000 vaddr=<private> PDP0=0x434FC9000
2026-09-28 03:21:20.614393+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x433F55000 ggttOffset=0x408C000
2026-09-28 03:21:20.614396+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x408C000 vaddr=<private> size=0x4000
2026-09-28 03:21:20.614398+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcBuildContext: state@+0x1000 ggtt=0x4004000 size=0x4000 CTL=0x1801 CC=0x90009
2026-09-28 03:21:20.614400+0700  localhost kernel[0]: (MyIntelGPU) Ring[RCS]: ringCreate: OK — ring ready at GGTT=0x4004000 vaddr=<private>
2026-09-28 03:21:20.614402+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2256] HW Accel: RCS ring created OK (ggttOffset=0x4004000, vaddr=<private>)
2026-09-28 03:21:20.614404+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2261] HW Accel: Allocating BCS ring buffer...
2026-09-28 03:21:20.620562+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x435160000 ggttOffset=0x4090000
2026-09-28 03:21:20.620568+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2281] HW Accel: Creating BCS ring (mmio_base=0x22000)...
2026-09-28 03:21:20.620570+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=BCS mmioBase=0x22000 size=16384
2026-09-28 03:21:20.626454+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x435171000 ggttOffset=0x4094000
2026-09-28 03:21:20.626458+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4094000 vaddr=<private> size=16384
2026-09-28 03:21:20.626704+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:21:20.626713+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:21:20.632597+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=4096 pages=1 cpuAddr=<private> physAddr=0x4346C1000 ggttOffset=0x4098000
2026-09-28 03:21:20.632619+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4098000 (STAM+TLB)
2026-09-28 03:21:20.632623+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:21:20.632628+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:21:20.632635+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:21:20.632637+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:21:20.632758+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:21:20.639593+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=536576 pages=131 cpuAddr=<private> physAddr=0x434161000 ggttOffset=0x4099000
2026-09-28 03:21:20.639615+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcAllocPPGTT: OK — 131 pages @ ggtt=0x4099000 vaddr=<private> PDP0=0x434161000
2026-09-28 03:21:20.645606+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x433429000 ggttOffset=0x411C000
2026-09-28 03:21:20.645610+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x411C000 vaddr=<private> size=0x4000
2026-09-28 03:21:20.645612+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcBuildContextXcs: state@+0x1000 ggtt=0x4094000 size=0x4000 CTL=0x1801 CC=0x90009 (52 dw)
2026-09-28 03:21:20.645614+0700  localhost kernel[0]: (MyIntelGPU) Ring[BCS]: ringCreate: OK — ring ready at GGTT=0x4094000 vaddr=<private>
2026-09-28 03:21:20.645616+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2295] HW Accel: BCS ring created OK (ggttOffset=0x4094000, vaddr=<private>)
2026-09-28 03:21:20.645623+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2310] HW Accel: Emitting RCS init commands...
2026-09-28 03:21:20.645710+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:21:20.646782+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:21:20.646796+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:21:20.646798+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x00000020
2026-09-28 03:21:20.646809+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:21:20.646811+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:21:20.646814+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008001
2026-09-28 03:21:20.646816+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8000
2026-09-28 03:21:20.646817+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646822+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646823+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646824+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646825+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646826+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646827+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646828+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646829+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646830+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.646831+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2323] HW Accel: RCS init commands submitted (tail=24)
2026-09-28 03:21:20.646833+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2327] HW Accel: Emitting BCS init commands...
2026-09-28 03:21:20.646837+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:21:20.647914+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:21:20.647924+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:21:20.647926+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x60000020
2026-09-28 03:21:20.647935+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:21:20.647937+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:21:20.647938+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008019
2026-09-28 03:21:20.647939+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8018
2026-09-28 03:21:20.647940+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647941+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647942+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647943+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647944+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647945+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647946+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647947+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647948+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647949+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:21:20.647950+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2334] HW Accel: BCS init commands submitted (tail=24)
2026-09-28 03:21:20.647952+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2339] === HW Accel Init OK (partial OK) ===
2026-09-28 03:21:20.647953+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3302] Phase 6b: HW Acceleration OK (RCS=yes BCS=yes)
2026-09-28 03:21:20.647955+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P6b
2026-09-28 03:21:20.647958+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [armEngineInterrupts:3816] armEngineInterrupts: SKIPPED (boot-arg -myintelgtirq not set and no display FB)
2026-09-28 03:21:20.647965+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: VRAM Pool report on GPU node — 4096 MB (default ON, opt-out myintelvram=0)
2026-09-28 03:21:20.647983+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3433] Phase 7: Kext start completed successfully
2026-09-28 03:21:20.647985+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P7-done
2026-09-28 03:21:20.860420+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:22:05.776984+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:22:05.777060+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:26:04.641407+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Volumes/Untitled/MyIntelGPU.kext
2026-09-28 03:26:04.642109+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Volumes/Untitled/MyIntelGPU.kext.bak.kext
2026-09-28 03:26:09.522865+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:26:09.523030+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:26:11.018242+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Volumes/Untitled/MyIntelGPU.kext
2026-09-28 03:26:11.021662+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Volumes/Untitled/MyIntelGPU.kext.bak.kext
2026-09-28 03:29:42.663665+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:29:42.664062+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:34:43.433790+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Library/Extensions/MyIntelGPU.kext
2026-09-28 03:35:22.043712+0700  localhost kernel[0]: KextLog: AuxKC bundle com.pongpan-bk.MyIntelGPU marked as loadable
2026-09-28 03:35:22.821570+0700  localhost kernel[0]: KextLog: AuxKC bundle com.pongpan-bk.MyIntelGPU marked as loadable
2026-09-28 03:35:50.333403+0700  localhost kernel[0]: KextLog: AuxKC bundle com.pongpan-bk.MyIntelGPU marked as loadable
2026-09-28 03:35:53.118469+0700  localhost kernel[0]: KextLog: AuxKC bundle com.pongpan-bk.MyIntelGPU marked as loadable
2026-09-28 03:36:48.839447+0700  localhost kernel[0]: Driver com.apple.DriverKit-IOUserDockChannelSerial has crashed 0 time(s)
2026-09-28 03:36:48.856751+0700  localhost kernel[0]: (MyIntelGPU) MyIntelFB: [init:69] init() OK - 1920x1080 @ 60Hz
2026-09-28 03:36:48.856773+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [init:286] init() — OK
2026-09-28 03:36:48.856775+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU v2.0.240 | First Custom iGPU Driver for Intel Core 5 120U on macOS
https://github.com/pongpan-bk/MyIntelGPU-MacDriver-HonestBridge
2026-09-28 03:36:48.856872+0700  localhost kernel[0]: (MyIntelGPU) MyIntelFB: [free:75] free()
2026-09-28 03:36:48.856883+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2853] Phase 1: PCI setup OK
2026-09-28 03:36:48.856885+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P1
2026-09-28 03:36:48.856887+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2925] BAR0 desc: length=0x1000000
2026-09-28 03:36:48.857090+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2960] Phase 2: BAR0 MMIO mapped at 0x<private> (size=0x1000000)
2026-09-28 03:36:48.857092+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P2
2026-09-28 03:36:48.857095+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:427] PCI DeviceID = 0xA7AC, Revision = 0x04
2026-09-28 03:36:48.857097+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:481] Detected Raptor Lake (Gen12) → Faking Coffee Lake (Gen9)
2026-09-28 03:36:48.857100+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [detectHardwareGeneration:547] GMD_ID not available — using PCI ID fallback
2026-09-28 03:36:48.857102+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:2976] Phase 3: Hardware detected — Gen=12, FakeGen=9
2026-09-28 03:36:48.857104+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P3
2026-09-28 03:36:48.857105+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3002] Device memory count: 3
2026-09-28 03:36:48.857108+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3022] BAR2: config phys=0x60000000 is64=1
2026-09-28 03:36:48.857110+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=0 tag=0x82001010 phys=0x51000000 len=0x1000000
2026-09-28 03:36:48.857112+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=1 tag=0xC2001018 phys=0x60000000 len=0x10000000
2026-09-28 03:36:48.857115+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3031] BAR2DBG: idx=2 tag=0x81001020 phys=0x4000 len=0x40
2026-09-28 03:36:48.859500+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3081] Phase 4: BAR2 aperture mapped at 0x<private> (size=0x10000000)
2026-09-28 03:36:48.859502+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P4
2026-09-28 03:36:48.859504+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:617] Building translation table (FakeGen=9, RealGen=12)
2026-09-28 03:36:48.859507+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:731] Translation table built with 6 entries:
2026-09-28 03:36:48.859509+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [0] RCS0: 0x2000 → 0x2000 [skipped]
2026-09-28 03:36:48.859511+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [1] BCS0 (Blitter): 0x22000 → 0x22000 [skipped]
2026-09-28 03:36:48.859513+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [2] VECS0 (Video Encode): 0x1A000 → 0x1C8000 [active]
2026-09-28 03:36:48.859520+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [3] Cursor Pipe B: 0x700C0 → 0x71080 [active]
2026-09-28 03:36:48.859522+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [4] PCH Display: 0x48000 → 0xC8000 [active]
2026-09-28 03:36:48.859523+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [buildTranslationTable:738]   [5] Cursor Pipe D: 0x73080 → 0x73080 [skipped]
2026-09-28 03:36:48.859525+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3103] Phase 5: Translation table ready (6 entries)
2026-09-28 03:36:48.859527+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5
2026-09-28 03:36:48.859529+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3138] Phase 5b: Skipping display init (gEnableDisplayFramebuffer=0 _fx_g9t=9)
2026-09-28 03:36:48.859531+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3165] Phase 5c: Creating IntelFramebuffer (no interrupts)...
2026-09-28 03:36:48.859532+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5c
2026-09-28 03:36:48.859535+0700  localhost kernel[0]: (MyIntelGPU) IntelFB: [init:57] init() — OK
2026-09-28 03:36:48.859536+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3179] Phase 5c: Initialized without Interrupts
2026-09-28 03:36:48.859537+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P5c-done
2026-09-28 03:36:48.859539+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3254] Phase 5d: SKIPPED MyIntelFramebuffer (GPU-only mode; pass -myintelfb to enable display)
2026-09-28 03:36:48.859541+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1223] GGTT: GMCH_CTRL=0x02C1 ggms=3
2026-09-28 03:36:48.859544+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1256] GGTT: GMCH_CTRL=0x02C1 size=8MB entries=1048576 GSM phys=0x51800000
2026-09-28 03:36:48.859637+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: fGsm=<private> gttTotal=1048576 (0x100000)
2026-09-28 03:36:48.859642+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[0..7]   = 000000004C800001 000000004C801001 000000004C802001 000000004C803001 000000004C804001 000000004C805001 000000004C806001 000000004C807001
2026-09-28 03:36:48.859646+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[2048]   = 000000004D000001  PTE[4096]  = 000000004D800001
2026-09-28 03:36:48.859648+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: PTE[65536]  = 0000000000000000  PTE[last-4] = 0000000000000000
2026-09-28 03:36:48.879251+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: non-zero in first 65536 PTEs = 16672
2026-09-28 03:36:48.884356+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: framebuffer PTE run ends at page 16384 (64 MB)
2026-09-28 03:36:48.884359+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: fb PTE run base=0x4C800000 pages=16384 — zeroing only, stolen comes from DSMBASE/GMS
2026-09-28 03:36:48.893154+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:36:48.919337+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:36:48.921797+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:36:48.975618+0700  localhost kernel[0]: (MyIntelGPU) GGTTDBG: zeroed GGTT pages [16384 .. 1048575)
2026-09-28 03:36:48.975634+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1381] VRAMPOOL: capacity=4096 MB (1048576 GTT pages) — report target 4096 MB
2026-09-28 03:36:48.975637+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [ggttInitHardware:1383] GGTT: OK — fGsm=<private> fGttTotal=1048576
2026-09-28 03:36:48.975640+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3271] Phase 6: GGTT init complete (entries=1048576, gsm=<private>)
2026-09-28 03:36:48.975642+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P6
2026-09-28 03:36:48.975647+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2151] === HW Accel Init Start ===
2026-09-28 03:36:48.975992+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:36:48.976001+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [disablePowerGating:2010] PG/RC6: Disabled (MISCCPCTL=0xFFFFFFFE RC_CONTROL=0x00000000)
2026-09-28 03:36:48.976004+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2194] HW Accel: Allocating RCS ring buffer...
2026-09-28 03:36:48.981250+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x430DF5000 ggttOffset=0x4000000
2026-09-28 03:36:48.981259+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2217] HW Accel: pre-ring FW_ACK_GT=0x00000000 GDRST=0x00000000 CORE_STATUS=0x10800303
2026-09-28 03:36:48.981265+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2232] HW Accel: pre-ring GUC_STATUS=0x00000000 (MIA_reset=0 bootrom=0x0 ukernel=0x0 mia=0x0 auth=0)
2026-09-28 03:36:48.981268+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2235] HW Accel: pre-ring WOPCM_SIZE=0x00000000 (locked=0) DMA_OFFSET=0x00000000 (valid=0)
2026-09-28 03:36:48.981479+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:36:48.981483+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2063] GT Reset: begin (GDRST before=0x00000000)
2026-09-28 03:36:48.981486+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2075] GT Reset: RCS RING_RESET_CTL=0x0 (req=0 ready=0 cat=0)
2026-09-28 03:36:48.981492+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2109] GT Reset: RCS ready-to-reset acked (RESET_CTL=0x3)
2026-09-28 03:36:48.981495+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2075] GT Reset: BCS RING_RESET_CTL=0x0 (req=0 ready=0 cat=0)
2026-09-28 03:36:48.981500+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2109] GT Reset: BCS ready-to-reset acked (RESET_CTL=0x3)
2026-09-28 03:36:48.981650+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2131] GT Reset: GDRST acked (GDRST after=0x00000000)
2026-09-28 03:36:48.981658+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2139] GT Reset: RCS REQUEST_RESET cleared (RESET_CTL=0x0)
2026-09-28 03:36:48.981663+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2139] GT Reset: BCS REQUEST_RESET cleared (RESET_CTL=0x0)
2026-09-28 03:36:48.981665+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [gtResetEngines:2142] GT Reset: done (ok=yes)
2026-09-28 03:36:48.981667+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2242] HW Accel: Creating RCS ring (mmio_base=0x2000)...
2026-09-28 03:36:48.981669+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=RCS mmioBase=0x2000 size=16384
2026-09-28 03:36:48.986871+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x43335C000 ggttOffset=0x4004000
2026-09-28 03:36:48.986875+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4004000 vaddr=<private> size=16384
2026-09-28 03:36:48.987084+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:36:48.987088+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:36:48.992309+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=4096 pages=1 cpuAddr=<private> physAddr=0x42D204000 ggttOffset=0x4008000
2026-09-28 03:36:48.992331+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4008000 (STAM+TLB)
2026-09-28 03:36:48.992337+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:36:48.992342+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:36:48.992350+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:36:48.992352+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:36:48.992506+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:36:48.998033+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=536576 pages=131 cpuAddr=<private> physAddr=0x42CA89000 ggttOffset=0x4009000
2026-09-28 03:36:48.998051+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcAllocPPGTT: OK — 131 pages @ ggtt=0x4009000 vaddr=<private> PDP0=0x42CA89000
2026-09-28 03:36:49.003003+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x4334E1000 ggttOffset=0x408C000
2026-09-28 03:36:49.003006+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x408C000 vaddr=<private> size=0x4000
2026-09-28 03:36:49.003008+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcBuildContext: state@+0x1000 ggtt=0x4004000 size=0x4000 CTL=0x1801 CC=0x90009
2026-09-28 03:36:49.003010+0700  localhost kernel[0]: (MyIntelGPU) Ring[RCS]: ringCreate: OK — ring ready at GGTT=0x4004000 vaddr=<private>
2026-09-28 03:36:49.003012+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2256] HW Accel: RCS ring created OK (ggttOffset=0x4004000, vaddr=<private>)
2026-09-28 03:36:49.003019+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2261] HW Accel: Allocating BCS ring buffer...
2026-09-28 03:36:49.008128+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x433565000 ggttOffset=0x4090000
2026-09-28 03:36:49.008134+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2281] HW Accel: Creating BCS ring (mmio_base=0x22000)...
2026-09-28 03:36:49.008136+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: engine=BCS mmioBase=0x22000 size=16384
2026-09-28 03:36:49.013435+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x434469000 ggttOffset=0x4094000
2026-09-28 03:36:49.013444+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: buffer @ ggtt=0x4094000 vaddr=<private> size=16384
2026-09-28 03:36:49.013685+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:36:49.013707+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_RESET_CTL before = 0x0 (req=0 ready=0 cat=0)
2026-09-28 03:36:49.018723+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=4096 pages=1 cpuAddr=<private> physAddr=0x4335E0000 ggttOffset=0x4098000
2026-09-28 03:36:49.018739+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: HWS Page @ ggtt=0x4098000 (STAM+TLB)
2026-09-28 03:36:49.018744+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_MODE_GEN7 = 0x0 (RUN_LIST=0)
2026-09-28 03:36:49.018749+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: DISABLE_LEGACY written, RING_MODE_GEN7 = 0x8 (bit3=1)
2026-09-28 03:36:49.018755+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_STATUS_LO=0x00000001 HI=0x00000000 (active=1 pend=0 load=0)
2026-09-28 03:36:49.018757+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: EXECLIST_SQ_CONTENTS[0]=0x00000000 [1]=0x00000000 (desc=0x0000000000000000)
2026-09-28 03:36:49.018881+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: RING_CTL_VALID not set via MMIO (expected on Gen12+, execlists mode), continuing
2026-09-28 03:36:49.024593+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=536576 pages=131 cpuAddr=<private> physAddr=0x42D318000 ggttOffset=0x4099000
2026-09-28 03:36:49.024625+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcAllocPPGTT: OK — 131 pages @ ggtt=0x4099000 vaddr=<private> PDP0=0x42D318000
2026-09-28 03:36:49.029681+0700  localhost kernel[0]: (MyIntelGPU) GEMBuf: gemBufferCreate: OK — size=16384 pages=4 cpuAddr=<private> physAddr=0x42B4B9000 ggttOffset=0x411C000
2026-09-28 03:36:49.029687+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringCreate: LRC context @ ggtt=0x411C000 vaddr=<private> size=0x4000
2026-09-28 03:36:49.029689+0700  localhost kernel[0]: (MyIntelGPU) Ring: lrcBuildContextXcs: state@+0x1000 ggtt=0x4094000 size=0x4000 CTL=0x1801 CC=0x90009 (52 dw)
2026-09-28 03:36:49.029692+0700  localhost kernel[0]: (MyIntelGPU) Ring[BCS]: ringCreate: OK — ring ready at GGTT=0x4094000 vaddr=<private>
2026-09-28 03:36:49.029695+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2295] HW Accel: BCS ring created OK (ggttOffset=0x4094000, vaddr=<private>)
2026-09-28 03:36:49.029697+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2310] HW Accel: Emitting RCS init commands...
2026-09-28 03:36:49.029787+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:36:49.030847+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:36:49.030857+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x000000200408C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:36:49.030860+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x00000020
2026-09-28 03:36:49.030871+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:36:49.030874+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:36:49.030876+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008001
2026-09-28 03:36:49.030878+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8000
2026-09-28 03:36:49.030879+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030881+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030882+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030883+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030885+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030886+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030887+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030888+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030890+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030891+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.030892+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2323] HW Accel: RCS init commands submitted (tail=24)
2026-09-28 03:36:49.030894+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2327] HW Accel: Emitting BCS init commands...
2026-09-28 03:36:49.030901+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [forceWakeGet:2552] ForceWake: domains awake (attempt 1: gt=0x00000001 render=0x00000001)
2026-09-28 03:36:49.031960+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: post-kick poll ACTIVE=1 CSBwr=0x1 HEAD=0x18 loops=1
2026-09-28 03:36:49.031968+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: desc=0x600000200411C11D tail=24 CSBwr=1 ACTIVE=1
2026-09-28 03:36:49.031970+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: EXECLIST_STATUS_HI=0x60000020
2026-09-28 03:36:49.031983+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: HEAD=0x18 TAIL=0x18 EIR=0x00000000 ESR=0x00000000 IPEIR=0x00000000 IPEHR=0x01000000
2026-09-28 03:36:49.031986+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: FAULT_GEN12=0x00000000
2026-09-28 03:36:49.031988+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[00]=0x03FF8000_00008019
2026-09-28 03:36:49.031989+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[01]=0x00008000_03FF8018
2026-09-28 03:36:49.031991+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[02]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031992+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[03]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031993+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[04]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031994+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[05]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031996+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[06]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031997+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[07]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031998+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[08]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.031999+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[09]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.032000+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[10]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.032002+0700  localhost kernel[0]: (MyIntelGPU) Ring: ringSubmitExeclists: CSB[11]=0xFFFFFFFF_FFFFFFFF
2026-09-28 03:36:49.032003+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2334] HW Accel: BCS init commands submitted (tail=24)
2026-09-28 03:36:49.032005+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [initHardwareAcceleration:2339] === HW Accel Init OK (partial OK) ===
2026-09-28 03:36:49.032007+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3302] Phase 6b: HW Acceleration OK (RCS=yes BCS=yes)
2026-09-28 03:36:49.032009+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P6b
2026-09-28 03:36:49.032021+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [armEngineInterrupts:3816] armEngineInterrupts: SKIPPED (boot-arg -myintelgtirq not set and no display FB)
2026-09-28 03:36:49.032027+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: VRAM Pool report on GPU node — 4096 MB (default ON, opt-out myintelvram=0)
2026-09-28 03:36:49.032057+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [start:3433] Phase 7: Kext start completed successfully
2026-09-28 03:36:49.032059+0700  localhost kernel[0]: (MyIntelGPU) MyIntelGPU: [progress] start:P7-done
2026-09-28 03:36:49.230017+0700  localhost kernel[0]: Driver com.apple.AppleUserHIDDrivers has crashed 0 time(s)
2026-09-28 03:37:16.801746+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(453) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:37:16.802063+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(453) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip

========================================================================
 8. auxKC DEPLOYMENT MATRIX & BOOT SNAPSHOT TAGS
========================================================================
--- Linker Dependencies Inspection ---
com.highpoint-tech.kext.HighPointIOP	4.4.5	/Library/Extensions/HighPointIOP.kext	
com.pongpan-bk.MyIntelGPU	3.1.38	/Library/Extensions/MyIntelGPU.kext	
com.realtek.driver.RtWlanU1827	1827.4.b36	/Library/Extensions/RtWlanU1827.kext	
com.apple.driver.AppleMobileDevice	4.0	/Library/Apple/System/Library/Extensions/AppleMobileDevice.kext	
com.highpoint-tech.kext.HighPointRR	4.22.1	/Library/Extensions/HighPointRR.kext	
com.realtek.driver.RtWlanU	1830.32.b27	/Library/Extensions/RtWlanU.kext	
--- Active APFS Boot Tag Graph ---
    Name:        com.apple.os.update-3DB6833B375FB4E5C41B37F9BAFAF44D774DF658373DFE1C2060C28A7EF59134
    XID:         328

========================================================================
 9. XNU CORE SAFETY & INTER-PROCESS SANDBOX POLICIES
========================================================================
2026-09-28 03:12:06.369821+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(429) deny(1) file-read-xattr /Library/Extensions/MyIntelGPU.kext
2026-09-28 03:13:13.770077+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(429) deny(1) file-read-xattr /Library/Extensions/MyIntelGPU.kext
2026-09-28 03:16:18.939655+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:16:18.940681+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:16:20.243668+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Library/Extensions/MyIntelGPU.kext
2026-09-28 03:18:14.979435+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:18:14.984909+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(462) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:22:05.776984+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:22:05.777060+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:26:04.641407+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Volumes/Untitled/MyIntelGPU.kext
2026-09-28 03:26:04.642109+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Volumes/Untitled/MyIntelGPU.kext.bak.kext
2026-09-28 03:26:09.522865+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:26:09.523030+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:26:11.018242+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Volumes/Untitled/MyIntelGPU.kext
2026-09-28 03:26:11.021662+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Volumes/Untitled/MyIntelGPU.kext.bak.kext
2026-09-28 03:29:42.663665+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip
2026-09-28 03:29:42.664062+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:34:43.433790+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(447) deny(1) file-read-xattr /Library/Extensions/MyIntelGPU.kext
2026-09-28 03:37:16.801746+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(453) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3 10.pkg
2026-09-28 03:37:16.802063+0700  localhost kernel[0]: (Sandbox) Sandbox: com.apple.quicklook.ThumbnailsAg(453) deny(1) file-read-xattr /Users/ppbk/Downloads/MyIntelGPU-3.1.3.pkg (9).zip

==========================================================================================
    [THE MASTER ENGINE COMPLETE] ALL LAYERS EXTRACTED | PONGPAN iGPU TECH LABS SECURED    
==========================================================================================

กดปุ่มใดๆ เพื่อปิดหน้าต่าง...

