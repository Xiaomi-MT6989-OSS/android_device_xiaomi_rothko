# SurfaceFlinger thread capacity

`task_profiles.json` installs at `/vendor/etc/task_profiles.json`. Android's
libprocessgroup loads vendor profiles after system/API profiles and replaces
same-name actions. Only SFMainPolicy and SFRenderEnginePolicy are overridden.
Both select the existing foreground cpuset; CPU masks, frequency selection,
scheduler priority and thermal controls are unchanged.

SurfaceFlinger::init applies SFMainPolicy after RenderEngine initialization.
RenderEngineThreaded::threadMain applies SFRenderEnginePolicy independently.
Both upstream defaults select system-background. Thus an init service-level
ProcessCapacityHigh is subsequently overridden. The previous device replacement
of surfaceflinger.rc was removed; the upstream service definition remains.

Stock OS3.0.307.0 system profiles also use system-background for these names;
its vendor task profile file has no SF override. Therefore this is an explicit
device performance adaptation, not a claim to restore missing stock bytes.
It follows the supported profile extension instead of modifying upstream code
or moving threads repeatedly from a daemon. Earlier runtime tests motivated
trying foreground; observed group membership alone does not prove smoother
frames. A new image must verify main/RenderEngine memberships, frame times,
power consumption and regression behavior. No runtime installation was done
as part of this source change.
