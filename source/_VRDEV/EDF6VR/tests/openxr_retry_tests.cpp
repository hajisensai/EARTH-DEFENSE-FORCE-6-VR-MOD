// Automatic VR start with the runtime installed but no headset attached (fake OpenXR dispatch, no runtime,
// no headset). A start that is told FORM_FACTOR_UNAVAILABLE keeps its instance, so the retries the plugin
// makes every 3 s on the game's threads only ask xrGetSystem again instead of negotiating the runtime and
// creating an instance each time (1.2 ms against 0.01 ms apiece with Virtual Desktop's runtime). Any other failure
// still tears everything down.
#include "../src/openxr_session.cpp"
#include <cstdio>

namespace {
int failures=0;
int getSystemCalls=0, destroyInstanceCalls=0;
XrResult systemAnswer=XR_ERROR_FORM_FACTOR_UNAVAILABLE;
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); ++failures; } } while(false)

XrResult XRAPI_PTR GetSystem(XrInstance instance,const XrSystemGetInfo* info,XrSystemId* system) {
    CHECK(instance==reinterpret_cast<XrInstance>(0x1234));
    CHECK(info && info->formFactor==XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY);
    ++getSystemCalls;
    if(system) *system=XR_NULL_SYSTEM_ID;
    return systemAnswer;
}
XrResult XRAPI_PTR DestroyInstance(XrInstance instance) {
    CHECK(instance==reinterpret_cast<XrInstance>(0x1234));
    ++destroyInstanceCalls;
    return XR_SUCCESS;
}
void Quiet(const char*) {}

// The state a start that got as far as an instance leaves behind: an instance and its entry points.
void GiveInstance() {
    edf6vr::g_instance=reinterpret_cast<XrInstance>(0x1234);
    edf6vr::g_api.getSystem=&GetSystem;
    edf6vr::g_api.destroyInstance=&DestroyInstance;
}
}  // namespace

int main() {
    using namespace edf6vr;
    GiveInstance();
    const unsigned createdBefore=g_instancesCreated;

    // No headset: refused, and the instance stays for the next attempt.
    systemAnswer=XR_ERROR_FORM_FACTOR_UNAVAILABLE;
    CHECK(!g_openxr.Start(&Quiet,XrMode::TrackingOnly,nullptr,nullptr));
    CHECK(getSystemCalls==1);
    CHECK(destroyInstanceCalls==0);
    CHECK(g_instance==reinterpret_cast<XrInstance>(0x1234));
    CHECK(g_api.getSystem==&GetSystem && g_api.destroyInstance==&DestroyInstance);
    CHECK(!g_openxr.Running());

    // The retries ask the same instance again: no runtime negotiation, no new instance.
    for(int retry=0;retry<5;++retry) CHECK(!g_openxr.Start(&Quiet,XrMode::TrackingOnly,nullptr,nullptr));
    CHECK(getSystemCalls==6);
    CHECK(destroyInstanceCalls==0);
    CHECK(g_instancesCreated==createdBefore);
    CHECK(g_instance==reinterpret_cast<XrInstance>(0x1234));

    // Any other answer (the runtime went away) tears the instance down as before 2.1.6.
    systemAnswer=XR_ERROR_INSTANCE_LOST;
    CHECK(!g_openxr.Start(&Quiet,XrMode::TrackingOnly,nullptr,nullptr));
    CHECK(getSystemCalls==7);
    CHECK(destroyInstanceCalls==1);
    CHECK(g_instance==XR_NULL_HANDLE);
    CHECK(g_api.getSystem==nullptr && g_api.destroyInstance==nullptr);
    CHECK(g_instancesCreated==createdBefore);

    printf("openxr retry: %d failures\n",failures);
    return failures?1:0;
}
