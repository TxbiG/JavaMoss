# JavaMoss API Reference

> **Complete-parity public API design for Java.** This document specifies the intended idiomatic API when the binding exposes the full Moss feature set. It is a language-facing design contract, not a statement that every projected member is implemented in the current wrapper.

## 1. API design principles

Moss C++/CMoss is the canonical feature and behaviour baseline. The language binding is a **native Java library**, not a mechanical translation of C++ identifiers. The FFI may use C-compatible handles and exported `Moss_*` symbols internally, but ordinary users should not see those details.

The public surface is a managed Java API. Use packages, PascalCase classes, lowerCamelCase methods, immutable option/descriptor objects, `Optional<T>` only for naturally absent values, and `AutoCloseable` for owned native resources.

The binding must preserve feature coverage and semantics, not the number or spelling of native functions. Several native calls may become one managed method, a property, an iterator, a scoped object or a language protocol. Conversely, an overloaded native function may become distinct named methods when the language does not support overloads cleanly.

### Non-negotiable rules

- Keep the complete native feature set: Core, Platform, GPU, Renderer, Audio, GUI, Physics, Networking, XR, Navigation, Geometry, Resources, Skeleton, TriangleSplitter and Variants.
- Expose resource objects rather than asking users to pass opaque pointers repeatedly.
- Hide allocator plumbing, user-data pointers, C calling conventions, struct packing and raw WASM/JNI/P/Invoke handles behind the binding layer.
- Preserve status-query meaning. `false`, `0`, an empty collection and an absent optional value are not interchangeable with an error.
- Use asynchronous operations only when the target runtime requires them. WebAssembly startup and browser fetches are asynchronous; ordinary native API calls are not made asynchronous merely for stylistic consistency.
- Document platform/backend feature gates. A declared API is not proof that a backend or optional transport is available in a particular build.

## 2. Package and module layout

```java
import java.nio.file.Path;
import java.time.Duration;
import moss.Application;
import moss.audio.Audio;
import moss.platform.Platform;
import moss.platform.Window;
import moss.platform.WindowOptions;
import moss.gpu.GpuDevice;
import moss.gpu.GpuDeviceDesc;
import moss.gpu.GpuBufferDesc;
import moss.gpu.GpuBufferUsage;
```

### Subsystem map

| Moss subsystem | Public module / facade | Responsibility |
|---|---|---|
| Core | `moss.core` | Foundational value types, logging, memory, profiler, jobs, streams and utilities. |
| Platform | `moss.platform` | Windows/surfaces, monitors, events, input, gamepads, sensors, cameras, storage and dialogs. |
| GPU | `moss.gpu` | Devices, buffers, textures, samplers, shaders, pipelines, command recording, swapchains and synchronization. |
| Renderer | `moss.renderer` | Frame lifecycle, materials, resource sets, framebuffers, meshes, models, fonts, sprites, cameras and debug draw. |
| Audio | `moss.audio` | Audio engine, sources, streams, channels, effects, listeners, acoustics, speaker and microphone capture. |
| GUI | `moss.gui` | Contexts, windows, containers, widgets, text/scalar input and drawing. |
| Physics | `moss.physics` | Bodies, shapes, filters, collisions, constraints, characters, vehicles, soft bodies and navigation integration. |
| Network | `moss.network` | Hosts, peers, packets, transports, HTTP/TLS, WebSocket and WebRTC where enabled. |
| XR | `moss.xr` | Runtime/session lifecycle, views, actions, tracking, haptics, hand/body/face tracking and composition. |

The public package/module root should follow the repository's established package metadata. The names in this document are the recommended public design; implementation namespaces can be adjusted to the actual package identifier without changing the member design.

## 3. Core language type system

| Native Moss concept | Java public representation | Design requirement |
|---|---|---|
| `bool` | `boolean` | Keep boolean status and query values boolean. |
| Fixed-width signed/unsigned integers | `byte/short/int/long` | Preserve width and unsigned semantics at the ABI boundary. |
| `size_t` | `long` | Must be large enough for the host architecture. |
| `float` / `double` | `float / double` | Do not narrow double precision silently. |
| UTF-8 text | `String` | Copy or borrow according to documented native lifetime; validate encoding. |
| Opaque native resource | `AutoCloseable class` | Never expose raw pointers as normal user-facing values. |
| Pointer + element count | `primitive arrays / ByteBuffer` | Validate length, element type and retention duration. |
| Function pointer + user data | `functional interface with JNI global ref` | Unregister before callback state is released. |

### Value types and descriptors

Vectors, matrices, quaternions, colours, bounds, planes, rectangles and descriptor types should be real Java values. Use value structs/records where copy semantics are cheap and safe; use resource classes only for objects with identity or native lifetime. Descriptor objects should have named fields/options instead of long sequences of positional primitives. Validate invalid enum combinations before crossing FFI.

## 4. Public resource model

| Moss native resource family | Recommended public type | Ownership model |
|---|---|---|
| Window / monitor / gamepad / camera / capture / storage | `Window`, `Monitor`, `Gamepad`, `Camera`, `Capture`, `Storage` | The application owns platform state; each opened resource has explicit close/dispose semantics. |
| GPU device / buffer / texture / sampler / shader / pipeline / command buffer / fence / semaphore | `GpuDevice`, `GpuBuffer`, `GpuTexture`, `GpuSampler`, `GpuShader`, `GpuPipeline`, `GpuCommandBuffer`, `GpuFence`, `GpuSemaphore` | Child objects retain or borrow their creating device; destroy children before device shutdown. |
| Renderer / material / framebuffer / mesh / model / font / sprite / batch | `Renderer`, `Material`, `Framebuffer`, `Mesh`, `Model`, `Font`, `Sprite`, `SpriteBatch` | Renderer-owned resources cannot outlive the renderer unless explicitly reference-counted. |
| Audio source / stream / listener / effect / microphone | `AudioSource`, `AudioStream`, `AudioListener`, `AudioEffect`, `Microphone` | Release sources, streams and listeners before audio shutdown. |
| GUI context | `GuiContext` | Context scopes widget calls; begin/end pairs are balanced by scoped helpers or callbacks. |
| Physics system / body / shape / constraint / character / vehicle / soft body | `PhysicsSystem`, `Body`, `Shape`, `Constraint`, `Character`, `Vehicle`, `SoftBody` | System and job/allocator dependencies outlive bodies and simulation work. |
| Network host / peer / packet / HTTP request / WebSocket / WebRTC | `NetworkHost`, `Peer`, `Packet`, `HttpRequest`, `WebSocket`, `WebRtcConnection` | Explicit ownership of hosts, packets, requests and connections. |
| XR instance / session / action set / action / hand / body / face | `XrRuntime`, `XrSession`, `XrActionSet`, `XrAction`, `HandTracker`, `BodyTracker`, `FaceTracker` | Session-bound objects cannot outlive the session/runtime they depend on. |

The exact implementation may consolidate thin native objects where that is safer (for example, a renderer may own framebuffers or a device may own command pools). The public API must still make ownership unambiguous.

## 5. Lifecycle, errors, callbacks and concurrency

### Resource lifetime

Owned handles implement `AutoCloseable`; use try-with-resources. Borrowed views are non-owning and should not implement `close()`. JNI global references for callbacks must be released on unregistration. Never call Java callbacks while holding native locks unless the threading contract explicitly permits it. Byte buffers and typed arrays must document whether data is copied or retained.

### Errors and results

Methods that cannot complete throw `MossException` (or a typed subclass). Use `Optional<T>` for a value that may legitimately be absent and primitive `boolean` for status queries. Preserve unsigned native bit patterns; expose helpers for unsigned comparison/conversion rather than silently narrowing values.

### Threading and callbacks

- State which thread owns window/event pumping, renderer submission, audio callbacks, physics updates and XR frame operations.
- Do not promise thread safety unless the native subsystem guarantees it.
- Keep callback state alive until native unregistration completes; no callback may run after disposal/close returns.
- Never allow a managed exception, panic or language-runtime longjmp to unwind through a C ABI frame.
- For asynchronous APIs, define cancellation, completion and disposal behaviour. Do not return a view over memory that is freed when the Promise/callback completes.

## 6. Idiomatic quick-start patterns

### Platform and event loop

```java
try (Application app = Application.create();
     Window window = app.platform().createWindow(
         WindowOptions.builder()
             .title("Moss application")
             .size(1280, 720)
             .build())) {

    while (!window.shouldClose()) {
        app.platform().pollEvents();
    }
} // Window and Application close deterministically.
```

### GPU resource creation

```java
try (var device = GpuDevice.create(new GpuDeviceDesc());
     var buffer = device.createBuffer(
         new GpuBufferDesc(vertices.length, GpuBufferUsage.VERTEX))) {
    buffer.upload(vertices);
}
```

### Renderer frame

```java
try (var frame = renderer.beginFrame()) {
    frame.setCamera(camera);
    frame.drawSprite(sprite);
    frame.drawText("Hello Moss", position, textStyle);
    frame.present();
}
```

### Audio playback

```java
Audio.initialize();
try (AudioSource source = Audio.loadWav(Path.of("assets/theme.wav"))) {
    source.play();
    Audio.update(deltaTime);
} finally {
    Audio.shutdown();
}
```

### GUI context and widgets

```java
try (GuiContext ui = GUI.createContext()) {
    ui.window("Inspector", panel -> {
        panel.text("Player");
        boolean enabled = panel.checkbox("Enabled", currentValue);
    });
}
```

### Physics world

```java
try (PhysicsSystem physics = PhysicsSystem.create(settings)) {
    try (Body body = physics.bodies().create(
            BodyOptions.dynamic(Shape.box(size)))) {
        physics.step(deltaTime);
        List<Contact> contacts = physics.query().contacts(body);
    }
}
```

### Networking

```java
try (NetworkHost host = Network.createHost(hostOptions)) {
    for (NetworkEvent event : host.pollEvents()) {
        switch (event.type()) {
            case CONNECTED -> { /* ... */ }
            case RECEIVED -> { /* ... */ }
            case DISCONNECTED -> { /* ... */ }
        }
    }
}
```

### XR session

```java
try (XrSession session = XR.createSession(renderer)) {
    while (session.pollEvents()) {
        if (session.beginFrame()) {
            List<XrView> views = session.locateViews();
            // Render the active views.
            session.endFrame();
        }
    }
}
```

The examples show the intended public shape and lifecycle rules. Names and overloads in the index below are the parity mapping target; implementation-specific initialization parameters can be added to the relevant options objects without exposing the FFI.

## 7. Complete public API projection index

The following table maps every exported callable/allocator callback in the supplied CMoss declaration catalogue to a proposed Java public entry point. The native name is included **only for traceability**. It is not the public name that developers should call. Target signatures use idiomatic types and hide receiver/context handles where those belong to an object.

- **1063 indexed entries:** 1,058 callable/constructor declarations and five allocator callback values.
- Duplicate native overloads are retained as separate rows where their parameters differ; the public API should resolve each to a distinct overload or a disambiguated method name.
- Native pointer/out-parameter patterns should become owned resource values, spans/slices, tuples/records or callback objects as appropriate to the language.

### `Moss_Audio.h` → `moss.audio`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_Init_Audio` | `Audio.initialize(...)` | `Audio.initialize() : void` | function |
| `Moss_Terminate_Audio` | `Audio.shutdown(...)` | `Audio.shutdown() : void` | function |
| `Moss_AudioUpdate` | `Audio.update(...)` | `Audio.update(float deltaTime) : void` | function |
| `Moss_AudioLoadWavFile` | `Audio.loadWavFile(...)` | `Audio.loadWavFile(Path path) : AudioSource` | function |
| `Moss_AudioLoadWav` | `Audio.loadWav(...)` | `Audio.loadWav() : AudioSource` | function |
| `Moss_AudioLoadOgg` | `Audio.loadOgg(...)` | `Audio.loadOgg(String filename, AudioLoadType type) : AudioSource` | function |
| `Moss_AudioLoadMP3` | `Audio.loadMp3(...)` | `Audio.loadMp3(String filename) : AudioSource` | function |
| `Moss_AudioSourceDestroy` | `AudioSource.close(...)` | `AudioSource.close() : void` | function |
| `Moss_AudioCaptureMicrophone` | `Audio.captureMicrophone(...)` | `public static AudioSource Audio.captureMicrophone(Microphone microphone)` | function |
| `Moss_AudioCreateEffect` (overload 1) | `Audio.createEffect(...)` | `Audio.createEffect(AudioEffectType type) : AudioEffect` | function |
| `Moss_AudioCreateEffect` (overload 2) | `AudioEffect.setParameter(...)` | `AudioEffect.setParameter(String paramName, float value) : void` | function |
| `Moss_AudioRemoveEffect` | `AudioEffect.close(...)` | `AudioEffect.close() : void` | function |
| `Moss_AudioCreateChannel` | `Audio.createChannel(...)` | `Audio.createChannel(ChannelId channel) : ChannelId` | function |
| `Audio_RemoveChannel` | `Audio.removeChannel(...)` | `Audio.removeChannel(ChannelId channel) : void` | function |
| `Moss_AudioGetMasterChannel` | `Audio.getMasterChannel(...)` | `Audio.getMasterChannel() : ChannelId` | function |
| `Moss_AudioSetChannelVolume` | `Audio.setChannelVolume(...)` | `Audio.setChannelVolume(ChannelId channel, float volume) : void` | function |
| `Moss_AudioSetChannelMute` | `Audio.setChannelMute(...)` | `Audio.setChannelMute(ChannelId channel, boolean mute) : void` | function |
| `Moss_AudioAddChannelEffect` | `Audio.addEffectToChannel(...)` | `Audio.addEffectToChannel(ChannelId channel, AudioEffect effect) : void` | function |
| `Moss_AudioRemoveChannelEffect` | `Audio.removeEffectFromChannel(...)` | `Audio.removeEffectFromChannel(ChannelId channel, AudioEffect effect) : void` | function |
| `Moss_AudioRemoveAllChannelEffects` | `Audio.clearChannelEffects(...)` | `Audio.clearChannelEffects(ChannelId channel) : void` | function |
| `Moss_AudioStreamCreate` | `Audio.createStream(...)` | `Audio.createStream() : AudioStream` | function |
| `Moss_AudioStreamPlay` | `AudioStream.play(...)` | `AudioStream.play() : void` | function |
| `Moss_AudioStreamStop` | `AudioStream.stop(...)` | `AudioStream.stop() : void` | function |
| `Moss_AudioStreamSetVolume` | `AudioStream.setVolume(...)` | `AudioStream.setVolume(float volume) : void` | function |
| `Moss_AudioStreamSetPitch` | `AudioStream.setPitch(...)` | `AudioStream.setPitch(float pitch) : void` | function |
| `Moss_AudioStreamSetPlaybackRate` | `AudioStream.setPlaybackRate(...)` | `AudioStream.setPlaybackRate(float rate) : void` | function |
| `Moss_AudioStreamSetPan` | `AudioStream.setPan(...)` | `AudioStream.setPan(float pan) : void` | function |
| `Moss_AudioStreamSetLoop` | `AudioStream.setLoop(...)` | `AudioStream.setLoop(boolean loop) : void` | function |
| `Moss_AudioStreamRemove` | `AudioStream.close(...)` | `AudioStream.close() : void` | function |
| `Moss_AudioStream2DCreate` | `Audio.createStreamDimension2D(...)` | `Audio.createStreamDimension2D() : AudioStreamDIMENSION2D` | function |
| `Moss_AudioStream2DPlay` | `AudioStreamDIMENSION2D.play(...)` | `AudioStreamDIMENSION2D.play() : void` | function |
| `Moss_AudioStream2DStop` | `AudioStreamDIMENSION2D.stop(...)` | `AudioStreamDIMENSION2D.stop() : void` | function |
| `Moss_AudioStream2DSetVolume` | `AudioStreamDIMENSION2D.setVolume(...)` | `AudioStreamDIMENSION2D.setVolume(float volume) : void` | function |
| `Moss_AudioStream2DSetPitch` | `AudioStreamDIMENSION2D.setPitch(...)` | `AudioStreamDIMENSION2D.setPitch(float pitch) : void` | function |
| `Moss_AudioStream2DSetPan` | `AudioStreamDIMENSION2D.setPan(...)` | `AudioStreamDIMENSION2D.setPan(float pan) : void` | function |
| `Moss_AudioStream2DSetLoop` | `AudioStreamDIMENSION2D.setLoop(...)` | `AudioStreamDIMENSION2D.setLoop(boolean loop) : void` | function |
| `Moss_AudioStream3DSetPosition` (overload 1) | `AudioStreamDIMENSION2D.streamDimension3SetPosition(...)` | `AudioStreamDIMENSION2D.streamDimension3SetPosition() : void` | function |
| `Moss_AudioStream3DSetVelocity` (overload 1) | `AudioStreamDIMENSION2D.streamDimension3SetVelocity(...)` | `AudioStreamDIMENSION2D.streamDimension3SetVelocity() : void` | function |
| `Moss_AudioStream3DSetMaxDistance` (overload 1) | `AudioStreamDIMENSION2D.streamDimension3SetMaxDistance(...)` | `AudioStreamDIMENSION2D.streamDimension3SetMaxDistance() : void` | function |
| `Moss_AudioStream2DRemove` | `AudioStreamDIMENSION2D.close(...)` | `AudioStreamDIMENSION2D.close() : void` | function |
| `Moss_AudioStream3DCreate` | `Audio.createStreamDimension3D(...)` | `Audio.createStreamDimension3D() : AudioStreamDIMENSION3D` | function |
| `Moss_AudioStream3DPlay` | `AudioStreamDIMENSION3D.play(...)` | `AudioStreamDIMENSION3D.play() : void` | function |
| `Moss_AudioStream3DStop` | `AudioStreamDIMENSION3D.stop(...)` | `AudioStreamDIMENSION3D.stop() : void` | function |
| `Moss_AudioStream3DSetVolume` | `AudioStreamDIMENSION3D.setVolume(...)` | `AudioStreamDIMENSION3D.setVolume(float volume) : void` | function |
| `Moss_AudioStream3DSetPitch` | `AudioStreamDIMENSION3D.setPitch(...)` | `AudioStreamDIMENSION3D.setPitch(float pitch) : void` | function |
| `Moss_AudioStream3DSetPlaybackRate` | `AudioStreamDIMENSION3D.setPlaybackRate(...)` | `AudioStreamDIMENSION3D.setPlaybackRate(float rate) : void` | function |
| `Moss_AudioStream3DSetPan` | `AudioStreamDIMENSION3D.setPan(...)` | `AudioStreamDIMENSION3D.setPan(float pan) : void` | function |
| `Moss_AudioStream3DSetLoop` | `AudioStreamDIMENSION3D.setLoop(...)` | `AudioStreamDIMENSION3D.setLoop(boolean loop) : void` | function |
| `Moss_AudioStream3DSetPosition` (overload 2) | `AudioStreamDIMENSION3D.setPosition(...)` | `AudioStreamDIMENSION3D.setPosition() : void` | function |
| `Moss_AudioStream3DSetVelocity` (overload 2) | `AudioStreamDIMENSION3D.setVelocity(...)` | `AudioStreamDIMENSION3D.setVelocity() : void` | function |
| `Moss_AudioStream3DSetMaxDistance` (overload 2) | `AudioStreamDIMENSION3D.setMaxDistance(...)` | `AudioStreamDIMENSION3D.setMaxDistance() : void` | function |
| `Moss_AudioStream3DSetDistanceModel` | `AudioStreamDIMENSION3D.setDistanceModel(...)` | `AudioStreamDIMENSION3D.setDistanceModel(DistanceModel model) : void` | function |
| `Moss_AudioStream3DRemove` | `AudioStreamDIMENSION3D.close(...)` | `AudioStreamDIMENSION3D.close() : void` | function |
| `Moss_AudioCreateAudioListener2D` | `Audio.createAudioListenerDimension2D(...)` | `Audio.createAudioListenerDimension2D(Float2 position) : AudioListenerDIMENSION2D` | function |
| `Moss_AudioCreateAudioListener3D` | `Audio.createAudioListenerDimension3D(...)` | `Audio.createAudioListenerDimension3D(Float3 position) : AudioListenerDIMENSION3D` | function |
| `Moss_AudioCreateRayAudioListener2D` | `Audio.createRayAudioListenerDimension2D(...)` | `Audio.createRayAudioListenerDimension2D(Float2 position) : RayAudioListenerDIMENSION2D` | function |
| `Moss_AudioCreateRayAudioListener3D` | `Audio.createRayAudioListenerDimension3D(...)` | `Audio.createRayAudioListenerDimension3D(Float3 position) : RayAudioListenerDIMENSION3D` | function |
| `Moss_AudioRemoveAudioListener2D` | `AudioListenerDIMENSION2D.close(...)` | `AudioListenerDIMENSION2D.close() : void` | function |
| `Moss_AudioRemoveAudioListener3D` | `AudioListenerDIMENSION3D.close(...)` | `AudioListenerDIMENSION3D.close() : void` | function |
| `Moss_AudioRemoveRayAudioListener2D` | `RayAudioListenerDIMENSION2D.close(...)` | `RayAudioListenerDIMENSION2D.close() : void` | function |
| `Moss_AudioRemoveRayAudioListener3D` | `RayAudioListenerDIMENSION3D.close(...)` | `RayAudioListenerDIMENSION3D.close() : void` | function |
| `Moss_AudioActivateAudioListener2D` | `AudioListenerDIMENSION2D.activate(...)` | `AudioListenerDIMENSION2D.activate(boolean activate) : void` | function |
| `Moss_AudioActivateAudioListener3D` | `AudioListenerDIMENSION3D.activate(...)` | `AudioListenerDIMENSION3D.activate(boolean activate) : void` | function |
| `Moss_AudioActivateRayAudioListener2D` | `RayAudioListenerDIMENSION2D.activate(...)` | `RayAudioListenerDIMENSION2D.activate(boolean activate) : void` | function |
| `Moss_AudioActivateRayAudioListener3D` | `RayAudioListenerDIMENSION3D.activate(...)` | `RayAudioListenerDIMENSION3D.activate(boolean activate) : void` | function |
| `Moss_AudioListenerSetOrientation` | `AudioListenerDIMENSION3D.listenerSetOrientation(...)` | `AudioListenerDIMENSION3D.listenerSetOrientation(Vec3 forward, Vec3 up) : void` | function |
| `Moss_AudioRayListener2DSetPhysics` | `RayAudioListenerDIMENSION2D.raySetPhysics(...)` | `RayAudioListenerDIMENSION2D.raySetPhysics(PhysicsSystem physics, AudioRaycastDIMENSION2DCallback raycast) : void` | function |
| `Moss_AudioRayListener3DSetPhysics` | `RayAudioListenerDIMENSION3D.raySetPhysics(...)` | `RayAudioListenerDIMENSION3D.raySetPhysics(PhysicsSystem physics, AudioRaycastDIMENSION3DCallback raycast) : void` | function |
| `Moss_AudioRayListener2DSetTraceSettings` | `RayAudioListenerDIMENSION2D.raySetTraceSettings(...)` | `RayAudioListenerDIMENSION2D.raySetTraceSettings(float maxDistance, int reflectionRays) : void` | function |
| `Moss_AudioRayListener3DSetTraceSettings` | `RayAudioListenerDIMENSION3D.raySetTraceSettings(...)` | `RayAudioListenerDIMENSION3D.raySetTraceSettings(float maxDistance, int reflectionRays) : void` | function |
| `Moss_AudioRayTrace2D` | `Audio.rayTraceDimension2D(...)` | `Audio.rayTraceDimension2D(AudioRayTraceDIMENSION2DDesc desc, AudioRayTraceResult outResult) : boolean` | function |
| `Moss_AudioRayTrace3D` | `Audio.rayTraceDimension3D(...)` | `Audio.rayTraceDimension3D(AudioRayTraceDIMENSION3DDesc desc, AudioRayTraceResult outResult) : boolean` | function |
| `Moss_AudioRayTraceFromListener2D` | `RayAudioListenerDIMENSION2D.rayTraceFrom(...)` | `RayAudioListenerDIMENSION2D.rayTraceFrom(Vec2 sourcePosition, AudioRayTraceResult outResult) : boolean` | function |
| `Moss_AudioRayTraceFromListener3D` | `RayAudioListenerDIMENSION3D.rayTraceFrom(...)` | `RayAudioListenerDIMENSION3D.rayTraceFrom(Vec3 sourcePosition, AudioRayTraceResult outResult) : boolean` | function |
| `Moss_AudioRayTraceComputeGain` | `AudioRayTraceResult.rayTraceComputeGain(...)` | `AudioRayTraceResult.rayTraceComputeGain() : float` | function |
| `Moss_IsSpeakerDeviceReady` | `Audio.isSpeakerDeviceReady(...)` | `Audio.isSpeakerDeviceReady() : boolean` | function |
| `Moss_AudioSpeakerOpen` | `Audio.speakerOpen(...)` | `Audio.speakerOpen() : void` | function |
| `Moss_AudioSpeakerPause` | `Audio.speakerPause(...)` | `Audio.speakerPause() : void` | function |
| `Moss_AudioSpeakerResume` | `Audio.speakerResume(...)` | `Audio.speakerResume() : void` | function |
| `Moss_AudioSpeakerIsPaused` | `Audio.speakerIsPaused(...)` | `Audio.speakerIsPaused() : boolean` | function |
| `Moss_AudioSelectSpeakerDevice` | `Audio.selectSpeakerDevice(...)` | `Audio.selectSpeakerDevice(int id) : boolean` | function |
| `Moss_GetCurrentSpeakerDeviceID` | `Audio.getCurrentSpeakerDeviceId(...)` | `Audio.getCurrentSpeakerDeviceId() : int` | function |
| `Moss_GetSpeakerDeviceName` | `Audio.getSpeakerDeviceName(...)` | `Audio.getSpeakerDeviceName(int id) : String` | function |
| `Moss_ListSpeakerDevices` | `Audio.listSpeakerDevices(...)` | `Audio.listSpeakerDevices() : int` | function |
| `Moss_MicrophoneGetDeviceCount` | `Audio.microphoneGetDeviceCount(...)` | `Audio.microphoneGetDeviceCount() : int` | function |
| `Moss_MicrophoneGetDeviceName` | `Audio.microphoneGetDeviceName(...)` | `Audio.microphoneGetDeviceName(int deviceIndex) : String` | function |
| `Moss_MicrophoneOpen` | `Audio.microphoneOpen(...)` | `Audio.microphoneOpen(MicrophoneDesc desc) : Microphone` | function |
| `Moss_MicrophoneClose` | `Microphone.close(...)` | `Microphone.close() : void` | function |
| `Moss_MicrophoneStart` | `Microphone.start(...)` | `Microphone.start() : boolean` | function |
| `Moss_MicrophoneStop` | `Microphone.stop(...)` | `Microphone.stop() : void` | function |
| `Moss_MicrophoneRead` | `Microphone.read(...)` | `Microphone.read(float[] outSamples, int maxFrames) : int` | function |
| `Moss_MicrophoneSetGain` | `Microphone.setGain(...)` | `Microphone.setGain(float gain) : void` | function |
| `Moss_MicrophoneGetSampleRate` | `Microphone.getSampleRate(...)` | `Microphone.getSampleRate() : int` | function |
| `Moss_MicrophoneGetChannels` | `Microphone.getChannels(...)` | `Microphone.getChannels() : int` | function |
| `Moss_MicrophoneSetCallback` | `Microphone.setCallback(...)` | `Microphone.setCallback(MicrophoneCallback callback) : void` | function |
| `Moss_MicrophoneGetLevels` | `Microphone.getLevels(...)` | `Microphone.getLevels() : MicrophoneLevels` | function |
| `Moss_MicrophoneGetLevelRMS` | `Microphone.getLevelRms(...)` | `Microphone.getLevelRms() : float` | function |
| `Moss_MicrophoneGetLevelPeak` | `Microphone.getLevelPeak(...)` | `Microphone.getLevelPeak() : float` | function |
| `Moss_MicrophoneGetSmoothedVolume` | `Microphone.getSmoothedVolume(...)` | `Microphone.getSmoothedVolume() : float` | function |
| `Moss_MicrophoneGetVoiceActivity` | `Microphone.getVoiceActivity(...)` | `Microphone.getVoiceActivity() : float` | function |
| `Moss_IsMicrophoneDeviceReady` | `Audio.isMicrophoneDeviceReady(...)` | `Audio.isMicrophoneDeviceReady() : boolean` | function |
| `Moss_AudioMicrophoneOpen` | `Audio.microphoneOpen(...)` | `Audio.microphoneOpen() : int` | function |
| `Moss_AudioMicrophoneClose` | `Audio.microphoneClose(...)` | `Audio.microphoneClose() : void` | function |
| `Moss_AudioMicrophonePlay` | `Audio.microphonePlay(...)` | `Audio.microphonePlay() : void` | function |
| `Moss_AudioMicrophoneStop` | `Audio.microphoneStop(...)` | `Audio.microphoneStop() : void` | function |
| `Moss_AudioMicrophoneID` | `Audio.microphoneId(...)` | `Audio.microphoneId() : int` | function |
| `Moss_AudioSelectMicrophoneDevice` | `Audio.selectMicrophoneDevice(...)` | `Audio.selectMicrophoneDevice(int id) : boolean` | function |
| `Moss_GetMicrophoneDeviceName` | `Audio.getMicrophoneDeviceName(...)` | `Audio.getMicrophoneDeviceName(int index) : String` | function |
| `Moss_ListMicrophoneDevices` | `Audio.listMicrophoneDevices(...)` | `Audio.listMicrophoneDevices() : int` | function |
| `Moss_AudioMicrophoneSetGain` | `Microphone.setGain(...)` | `Microphone.setGain(float gain) : void` | function |
| `Moss_AudioMicrophoneGetSampleRate` | `Microphone.getSampleRate(...)` | `Microphone.getSampleRate() : int` | function |
| `Moss_AudioMicrophoneGetChannels` | `Microphone.getChannels(...)` | `Microphone.getChannels() : int` | function |
| `Moss_AudioStreamSetCallback` | `AudioStream.setCallback(...)` | `AudioStream.setCallback(AudioStreamCallback callback) : void` | function |

### `Moss_GPU.h` → `moss.gpu`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_AcquireGPUCommandBuffer` | `GpuDevice.acquireCommandBuffer(...)` | `GpuDevice.acquireCommandBuffer() : GpuCommandBuffer` | function |
| `Moss_BeginGPUComputePass` | `GpuCommandBuffer.beginComputePass(...)` | `GpuCommandBuffer.beginComputePass() : void` | function |
| `Moss_BeginGPUCopyPass` | `GpuCommandBuffer.beginCopyPass(...)` | `GpuCommandBuffer.beginCopyPass() : void` | function |
| `Moss_CreateGPUBuffer` | `GpuDevice.createBuffer(...)` | `GpuDevice.createBuffer(GpuBufferDesc desc) : GpuBuffer` | function |
| `Moss_DestroyGPUDevice` | `GpuDevice.close(...)` | `GpuDevice.close() : void` | function |
| `Moss_BeginGPURenderPass` | `GpuCommandBuffer.beginRenderPass(...)` | `GpuCommandBuffer.beginRenderPass(Framebuffer framebuffer) : void` | function |
| `Moss_BindGPUComputeSamplers` | `GpuCommandBuffer.bindComputeSamplers(...)` | `GpuCommandBuffer.bindComputeSamplers(int firstSlot, List<GpuSampler*> samplers) : void` | function |
| `Moss_BindGPUComputeTextures` | `GpuCommandBuffer.bindComputeTextures(...)` | `GpuCommandBuffer.bindComputeTextures(int firstSlot, List<GpuTexture*> textures) : void` | function |
| `Moss_BindGPUComputeUniformBuffers` | `GpuCommandBuffer.bindComputeUniformBuffers(...)` | `GpuCommandBuffer.bindComputeUniformBuffers(int firstSlot, List<GpuBufferBinding> bindings) : void` | function |
| `Moss_BindGPUFragmentSamplers` | `GpuCommandBuffer.bindFragmentSamplers(...)` | `GpuCommandBuffer.bindFragmentSamplers(int firstSlot, List<GpuSampler*> samplers) : void` | function |
| `Moss_BindGPUFragmentTextures` | `GpuCommandBuffer.bindFragmentTextures(...)` | `GpuCommandBuffer.bindFragmentTextures(int firstSlot, List<GpuTexture*> textures) : void` | function |
| `Moss_BindGPUFragmentUniformBuffers` | `GpuCommandBuffer.bindFragmentUniformBuffers(...)` | `GpuCommandBuffer.bindFragmentUniformBuffers(int firstSlot, List<GpuBufferBinding> bindings) : void` | function |
| `Moss_BindGPUIndexBuffer` | `GpuCommandBuffer.bindIndex(...)` | `GpuCommandBuffer.bindIndex(GpuBuffer buffer, long offset, GpuIndexType indexType) : void` | function |
| `Moss_BindGPUPipeline` | `GpuCommandBuffer.bindPipeline(...)` | `GpuCommandBuffer.bindPipeline(PipelineState pipeline) : void` | function |
| `Moss_BindGPUResourceSet` | `GpuCommandBuffer.bindResourceSet(...)` | `GpuCommandBuffer.bindResourceSet(int setIndex, ResourceSet set) : void` | function |
| `Moss_BindGPUVertexBuffers` | `GpuCommandBuffer.bindVertexBuffers(...)` | `GpuCommandBuffer.bindVertexBuffers(int firstBinding, List<GpuBufferBinding> bindings) : void` | function |
| `Moss_BindGPUVertexSamplers` | `GpuCommandBuffer.bindVertexSamplers(...)` | `GpuCommandBuffer.bindVertexSamplers(int firstSlot, List<GpuSampler*> samplers) : void` | function |
| `Moss_BindGPUVertexTextures` | `GpuCommandBuffer.bindVertexTextures(...)` | `GpuCommandBuffer.bindVertexTextures(int firstSlot, List<GpuTexture*> textures) : void` | function |
| `Moss_BindGPUVertexUniformBuffers` | `GpuCommandBuffer.bindVertexUniformBuffers(...)` | `GpuCommandBuffer.bindVertexUniformBuffers(int firstSlot, List<GpuBufferBinding> bindings) : void` | function |
| `Moss_BlitGPUTexture` | `GpuCommandBuffer.blitTexture(...)` | `GpuCommandBuffer.blitTexture(Texture src, Texture dst, GpuBlitRegion region) : void` | function |
| `Moss_CalculateGPUTextureFormatSize` | `GPU.calculateTextureFormatSize(...)` | `GPU.calculateTextureFormatSize(TextureFormat format, int width, int height, int depth) : int` | function |
| `Moss_CancelGPUCommandBuffer` | `GpuCommandBuffer.cancel(...)` | `GpuCommandBuffer.cancel() : void` | function |
| `Moss_CopyGPUBufferToBuffer` | `GpuCommandBuffer.copyToBuffer(...)` | `GpuCommandBuffer.copyToBuffer(GpuBuffer src, GpuBuffer dst, GpuBufferRegion srcRegion, GpuBufferRegion dstRegion) : void` | function |
| `Moss_CopyGPUBufferToTexture` | `GpuCommandBuffer.copyToTexture(...)` | `GpuCommandBuffer.copyToTexture(GpuBuffer src, GpuTexture dst, GpuBufferRegion srcRegion, GpuTextureRegion dstRegion) : void` | function |
| `Moss_CopyGPUTextureToBuffer` | `GpuCommandBuffer.copyTextureTo(...)` | `GpuCommandBuffer.copyTextureTo(GpuTexture src, GpuBuffer dst, GpuTextureRegion srcRegion, GpuBufferRegion dstRegion) : void` | function |
| `Moss_CopyGPUTextureToTexture` | `GpuCommandBuffer.copyTextureToTexture(...)` | `GpuCommandBuffer.copyTextureToTexture(GpuTexture src, GpuTexture dst, GpuTextureRegion srcRegion, GpuTextureRegion dstRegion) : void` | function |
| `Moss_CreateGPUSampler` | `GpuDevice.createSampler(...)` | `GpuDevice.createSampler(GpuSamplerCreateInfo createInfo) : GpuSampler` | function |
| `Moss_CreateGPUShader` | `GpuDevice.createShader(...)` | `GpuDevice.createShader(GpuShaderCreateInfo createInfo) : GpuShader` | function |
| `Moss_CreateGPUComputePipeline` | `GpuDevice.createComputePipeline(...)` | `GpuDevice.createComputePipeline(GpuComputePipelineCreateInfo createInfo) : ComputePipelineState` | function |
| `Moss_CreateGPUTexture` | `GpuDevice.createTexture(...)` | `GpuDevice.createTexture(GpuTextureCreateInfo createInfo) : GpuTexture` | function |
| `Moss_CreateGPUTextureView` | `GpuDevice.createTextureView(...)` | `GpuDevice.createTextureView(GpuTextureViewCreateInfo createInfo) : GpuTextureView` | function |
| `Moss_CreateGPUTransferBuffer` | `GpuDevice.createTransferBuffer(...)` | `GpuDevice.createTransferBuffer(GpuTransferBufferCreateInfo createInfo) : GpuTransferBuffer` | function |
| `Moss_CreateGPUFramebuffer` | `GpuDevice.createFramebuffer(...)` | `GpuDevice.createFramebuffer(GpuFramebufferCreateInfo createInfo) : Framebuffer` | function |
| `Moss_CreateGPUResourceSetLayout` | `GpuDevice.createResourceSetLayout(...)` | `GpuDevice.createResourceSetLayout(GpuResourceSetLayoutCreateInfo createInfo) : ResourceSetLayout` | function |
| `Moss_CreateGPUResourceSet` | `GpuDevice.createResourceSet(...)` | `GpuDevice.createResourceSet(GpuResourceSetCreateInfo createInfo) : ResourceSet` | function |
| `Moss_CreateGPUQueryPool` | `GpuDevice.createQueryPool(...)` | `GpuDevice.createQueryPool(GpuQueryPoolCreateInfo createInfo) : GpuQueryPool` | function |
| `Moss_DispatchGPUCompute` | `GpuCommandBuffer.dispatchCompute(...)` | `GpuCommandBuffer.dispatchCompute(int groupCountX, int groupCountY, int groupCountZ) : void` | function |
| `Moss_DispatchGPUComputeIndirect` | `GpuCommandBuffer.dispatchComputeIndirect(...)` | `GpuCommandBuffer.dispatchComputeIndirect(GpuBuffer indirectBuffer, long offset) : void` | function |
| `Moss_DownloadFromGPUBuffer` | `GpuBuffer.downloadFrom(...)` | `GpuBuffer.downloadFrom(ByteBuffer dstData, long size, long srcOffset) : void` | function |
| `Moss_DownloadFromGPUTexture` | `GpuTexture.downloadFrom(...)` | `GpuTexture.downloadFrom(GpuTextureRegion srcRegion, ByteBuffer dstData, int dstRowPitch, int dstSlicePitch) : void` | function |
| `Moss_DrawGPUIndexedPrimitives` | `GpuCommandBuffer.drawIndexedPrimitives(...)` | `GpuCommandBuffer.drawIndexedPrimitives(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) : void` | function |
| `Moss_DrawGPUIndexedPrimitivesIndirect` | `GpuCommandBuffer.drawIndexedPrimitivesIndirect(...)` | `GpuCommandBuffer.drawIndexedPrimitivesIndirect(GpuBuffer indirectBuffer, long offset, int drawCount, int stride) : void` | function |
| `Moss_DrawGPUPrimitives` | `GpuCommandBuffer.drawPrimitives(...)` | `GpuCommandBuffer.drawPrimitives(int vertexCount, int instanceCount, int firstVertex, int firstInstance) : void` | function |
| `Moss_DrawGPUPrimitivesIndirect` | `GpuCommandBuffer.drawPrimitivesIndirect(...)` | `GpuCommandBuffer.drawPrimitivesIndirect(GpuBuffer indirectBuffer, long offset, int drawCount, int stride) : void` | function |
| `Moss_EndGPUComputePass` | `GpuCommandBuffer.endComputePass(...)` | `GpuCommandBuffer.endComputePass() : void` | function |
| `Moss_EndGPUCopyPass` | `GpuCommandBuffer.endCopyPass(...)` | `GpuCommandBuffer.endCopyPass() : void` | function |
| `Moss_EndGPURenderPass` | `GpuCommandBuffer.endRenderPass(...)` | `GpuCommandBuffer.endRenderPass() : void` | function |
| `Moss_GDKResumeGPU` | `GpuDevice.gdkResume(...)` | `GpuDevice.gdkResume() : void` | function |
| `Moss_GDKSuspendGPU` | `GpuDevice.gdkSuspend(...)` | `GpuDevice.gdkSuspend() : void` | function |
| `Moss_GenerateMipmapsForGPUTexture` | `GpuCommandBuffer.generateMipmapsForTexture(...)` | `GpuCommandBuffer.generateMipmapsForTexture(GpuTexture texture) : void` | function |
| `Moss_GetGPUDriver` | `GPU.getDriver(...)` | `GPU.getDriver(int driverIndex) : String` | function |
| `Moss_GetCompiledGPUBackendType` | `GPU.getCompiledBackendType(...)` | `GPU.getCompiledBackendType() : GpuBackendType` | function |
| `Moss_GetGPUDeviceBackendType` | `GpuDevice.getBackendType(...)` | `GpuDevice.getBackendType() : GpuBackendType` | function |
| `Moss_GPUBackendGetName` | `GPU.backendGetName(...)` | `GPU.backendGetName(GpuBackendType backend) : String` | function |
| `Moss_GPUBackendGetShaderFormats` | `GPU.backendGetShaderFormats(...)` | `GPU.backendGetShaderFormats(GpuBackendType backend) : int` | function |
| `Moss_GPUBackendIsCompiled` | `GPU.backendIsCompiled(...)` | `GPU.backendIsCompiled(GpuBackendType backend) : boolean` | function |
| `Moss_GPUBackendIsSupportedOnPlatform` | `GPU.backendIsSupportedOnPlatform(...)` | `GPU.backendIsSupportedOnPlatform(GpuBackendType backend) : boolean` | function |
| `Moss_GetGPUBackendInfo` | `GPU.getBackendInfo(...)` | `GPU.getBackendInfo(GpuBackendType backend, GpuBackendInfo outInfo) : boolean` | function |
| `Moss_RegisterNativeGPUBackend` | `GPU.registerNativeBackend(...)` | `GPU.registerNativeBackend(GpuBackendType backend, ByteBuffer nativeTable) : boolean` | function |
| `Moss_HasNativeGPUBackend` | `GPU.hasNativeBackend(...)` | `GPU.hasNativeBackend(GpuBackendType backend) : boolean` | function |
| `Moss_GetGPUShaderFormats` | `GpuDevice.getShaderFormats(...)` | `GpuDevice.getShaderFormats() : int` | function |
| `Moss_GetGPUTextureFormatFromPixelFormat` | `GPU.getTextureFormatFromPixelFormat(...)` | `GPU.getTextureFormatFromPixelFormat(PixelFormat pixelFormat) : TextureFormat` | function |
| `Moss_GetNumGPUDrivers` | `GPU.getNumDrivers(...)` | `GPU.getNumDrivers() : int` | function |
| `Moss_GetPixelFormatFromGPUTextureFormat` | `GPU.getPixelFormatFromTextureFormat(...)` | `GPU.getPixelFormatFromTextureFormat(TextureFormat textureFormat) : PixelFormat` | function |
| `Moss_GPUSupportsProperties` | `GpuDevice.supportsProperties(...)` | `GpuDevice.supportsProperties() : boolean` | function |
| `Moss_GPUSupportsShaderFormats` | `GpuDevice.supportsShaderFormats(...)` | `GpuDevice.supportsShaderFormats(int shaderFormatMask) : boolean` | function |
| `Moss_GPUTextureFormatTexelBlockSize` | `GPU.textureFormatTexelBlockSize(...)` | `GPU.textureFormatTexelBlockSize(TextureFormat format) : int` | function |
| `Moss_GPUTextureSupportsFormat` | `GpuDevice.textureSupportsFormat(...)` | `GpuDevice.textureSupportsFormat(TextureFormat format, TextureUsage usage) : boolean` | function |
| `Moss_GPUTextureSupportsSampleCount` | `GpuDevice.textureSupportsSampleCount(...)` | `GpuDevice.textureSupportsSampleCount(TextureFormat format, int sampleCount) : boolean` | function |
| `Moss_InsertGPUDebugLabel` | `GpuCommandBuffer.insertDebugLabel(...)` | `GpuCommandBuffer.insertDebugLabel(String label) : void` | function |
| `Moss_MapGPUTransferBuffer` | `GpuTransferBuffer.map(...)` | `GpuTransferBuffer.map() : ByteBuffer` | function |
| `Moss_PopGPUDebugGroup` | `GpuCommandBuffer.popDebugGroup(...)` | `GpuCommandBuffer.popDebugGroup() : void` | function |
| `Moss_PushGPUComputeUniformData` | `GpuCommandBuffer.pushComputeUniformData(...)` | `GpuCommandBuffer.pushComputeUniformData(int slot, ByteBuffer data, int size) : void` | function |
| `Moss_PushGPUDebugGroup` | `GpuCommandBuffer.pushDebugGroup(...)` | `GpuCommandBuffer.pushDebugGroup(String label) : void` | function |
| `Moss_PushGPUFragmentUniformData` | `GpuCommandBuffer.pushFragmentUniformData(...)` | `GpuCommandBuffer.pushFragmentUniformData(int slot, ByteBuffer data, int size) : void` | function |
| `Moss_PushGPUVertexUniformData` | `GpuCommandBuffer.pushVertexUniformData(...)` | `GpuCommandBuffer.pushVertexUniformData(int slot, ByteBuffer data, int size) : void` | function |
| `Moss_QueryGPUFence` | `GpuFence.query(...)` | `GpuFence.query() : boolean` | function |
| `Moss_ReleaseGPUBuffer` | `GpuBuffer.release(...)` | `GpuBuffer.release() : void` | function |
| `Moss_ReleaseGPUFence` | `GpuFence.release(...)` | `GpuFence.release() : void` | function |
| `Moss_ReleaseGPUSampler` | `GpuSampler.release(...)` | `GpuSampler.release() : void` | function |
| `Moss_ReleaseGPUShader` | `GpuShader.release(...)` | `GpuShader.release() : void` | function |
| `Moss_ReleaseGPUComputePipeline` | `GpuDevice.releaseComputePipeline(...)` | `GpuDevice.releaseComputePipeline(ComputePipelineState pipeline) : void` | function |
| `Moss_ReleaseGPUTexture` | `GpuTexture.release(...)` | `GpuTexture.release() : void` | function |
| `Moss_ReleaseGPUTextureView` | `GpuTextureView.release(...)` | `GpuTextureView.release() : void` | function |
| `Moss_ReleaseGPUTransferBuffer` | `GpuTransferBuffer.release(...)` | `GpuTransferBuffer.release() : void` | function |
| `Moss_ReleaseGPUFramebuffer` | `Framebuffer.release(...)` | `Framebuffer.release() : void` | function |
| `Moss_ReleaseGPUResourceSetLayout` | `ResourceSetLayout.release(...)` | `ResourceSetLayout.release() : void` | function |
| `Moss_ReleaseGPUResourceSet` | `ResourceSet.release(...)` | `ResourceSet.release() : void` | function |
| `Moss_ReleaseGPUQueryPool` | `GpuQueryPool.release(...)` | `GpuQueryPool.release() : void` | function |
| `Moss_SetGPUAllowedFramesInFlight` | `GpuDevice.setAllowedFramesInFlight(...)` | `GpuDevice.setAllowedFramesInFlight(int framesInFlight) : void` | function |
| `Moss_SetGPUBlendConstants` | `GpuCommandBuffer.setBlendConstants(...)` | `GpuCommandBuffer.setBlendConstants(float r, float g, float b, float a) : void` | function |
| `Moss_SetGPUBufferName` | `GpuBuffer.setName(...)` | `GpuBuffer.setName(String name) : void` | function |
| `Moss_SetGPUScissor` (overload 1) | `GpuCommandBuffer.setScissor(...)` | `GpuCommandBuffer.setScissor(Rect scissor) : void` | function |
| `Moss_SetGPUStencilReference` | `GpuCommandBuffer.setStencilReference(...)` | `GpuCommandBuffer.setStencilReference(int reference) : void` | function |
| `Moss_SetGPUTextureName` | `GpuTexture.setName(...)` | `GpuTexture.setName(String name) : void` | function |
| `Moss_SetGPUViewport` | `GpuCommandBuffer.setViewport(...)` | `GpuCommandBuffer.setViewport(GpuViewport viewport) : void` | function |
| `Moss_SetGPUScissor` (overload 2) | `GpuCommandBuffer.setScissor(...)` | `GpuCommandBuffer.setScissor(Rect scissors) : void` | function |
| `Moss_SubmitGPUCommandBuffer` | `GpuCommandBuffer.submit(...)` | `GpuCommandBuffer.submit() : void` | function |
| `Moss_SubmitGPUCommandBufferAndAcquireFence` | `GpuCommandBuffer.submitAndAcquireFence(...)` | `GpuCommandBuffer.submitAndAcquireFence() : GpuFence` | function |
| `Moss_BindGPUComputePipeline` | `GpuCommandBuffer.bindComputePipeline(...)` | `GpuCommandBuffer.bindComputePipeline(ComputePipelineState pipeline) : void` | function |
| `Moss_BarrierGPUResources` | `GpuCommandBuffer.barrierResources(...)` | `GpuCommandBuffer.barrierResources(GpuBarrierInfo barrierInfo) : void` | function |
| `Moss_ResetGPUQueryPool` | `GpuCommandBuffer.resetQueryPool(...)` | `GpuCommandBuffer.resetQueryPool(GpuQueryPool queryPool, int firstQuery, int queryCount) : void` | function |
| `Moss_WriteGPUTimestamp` | `GpuCommandBuffer.writeTimestamp(...)` | `GpuCommandBuffer.writeTimestamp(GpuQueryPool queryPool, int queryIndex) : void` | function |
| `Moss_BeginGPUQuery` | `GpuCommandBuffer.beginQuery(...)` | `GpuCommandBuffer.beginQuery(GpuQueryPool queryPool, int queryIndex) : void` | function |
| `Moss_EndGPUQuery` | `GpuCommandBuffer.endQuery(...)` | `GpuCommandBuffer.endQuery(GpuQueryPool queryPool, int queryIndex) : void` | function |
| `Moss_GetGPUQueryResults` | `GpuQueryPool.getQueryResults(...)` | `GpuQueryPool.getQueryResults(int firstQuery, int queryCount, ByteBuffer data, long dataSize, long stride, GpuQueryResultFlags flags) : boolean` | function |
| `Moss_TransitionGPUBuffer` | `GpuCommandBuffer.transition(...)` | `GpuCommandBuffer.transition(GpuBuffer buffer, ResourceState oldState, ResourceState newState) : void` | function |
| `Moss_TransitionGPUTexture` | `GpuCommandBuffer.transitionTexture(...)` | `GpuCommandBuffer.transitionTexture(GpuTexture texture, ResourceState oldState, ResourceState newState) : void` | function |
| `Moss_UnmapGPUTransferBuffer` | `GpuTransferBuffer.unmap(...)` | `GpuTransferBuffer.unmap() : void` | function |
| `Moss_UploadToGPUBuffer` | `GpuBuffer.uploadTo(...)` | `GpuBuffer.uploadTo(ByteBuffer srcData, long size, long dstOffset) : void` | function |
| `Moss_UploadToGPUTexture` | `GpuTexture.uploadTo(...)` | `GpuTexture.uploadTo(GpuTextureTransferInfo transferInfo) : void` | function |
| `Moss_WaitForGPUFences` | `GpuDevice.waitForFences(...)` | `GpuDevice.waitForFences(List<GpuFence*> fences, boolean waitAll, long timeoutNs) : void` | function |
| `Moss_WaitForGPUIdle` | `GpuDevice.waitForIdle(...)` | `GpuDevice.waitForIdle() : void` | function |
| `Moss_WindowSupportsGPUPresentMode` | `Window.supportsPresentMode(...)` | `Window.supportsPresentMode(GpuPresentMode presentMode) : boolean` | function |
| `Moss_GPUBlitInfo` | `new GpuBlitInfo(...)` | `new GpuBlitInfo()` | constructor |
| `Moss_GPUBlitRegion` | `new GpuBlitRegion(...)` | `new GpuBlitRegion()` | constructor |
| `Moss_GPUBufferBinding` | `new GpuBufferBinding(...)` | `new GpuBufferBinding()` | constructor |
| `Moss_GPUBufferCreateInfo` | `new GpuBufferCreateInfo(...)` | `new GpuBufferCreateInfo()` | constructor |
| `Moss_GPUBufferLocation` | `new GpuBufferLocation(...)` | `new GpuBufferLocation()` | constructor |
| `Moss_GPUBufferRegion` | `new GpuBufferRegion(...)` | `new GpuBufferRegion()` | constructor |
| `Moss_GPUColorTargetBlendState` | `new GpuColorTargetBlendState(...)` | `new GpuColorTargetBlendState()` | constructor |
| `Moss_GPUColorTargetDescription` | `new GpuColorTargetDescription(...)` | `new GpuColorTargetDescription()` | constructor |
| `Moss_GPUColorTargetInfo` | `new GpuColorTargetInfo(...)` | `new GpuColorTargetInfo()` | constructor |
| `Moss_GPUDepthStencilState` | `new GpuDepthStencilState(...)` | `new GpuDepthStencilState()` | constructor |
| `Moss_GPUDepthStencilTargetInfo` | `new GpuDepthStencilTargetInfo(...)` | `new GpuDepthStencilTargetInfo()` | constructor |
| `Moss_GPUIndexedIndirectDrawCommand` | `new GpuIndexedIndirectDrawCommand(...)` | `new GpuIndexedIndirectDrawCommand()` | constructor |
| `Moss_GPUIndirectDispatchCommand` | `new GpuIndirectDispatchCommand(...)` | `new GpuIndirectDispatchCommand()` | constructor |
| `Moss_GPUIndirectDrawCommand` | `new GpuIndirectDrawCommand(...)` | `new GpuIndirectDrawCommand()` | constructor |
| `Moss_GPUMultisampleState` | `new GpuMultisampleState(...)` | `new GpuMultisampleState()` | constructor |
| `Moss_GPURasterizerState` | `new GpuRasterizerState(...)` | `new GpuRasterizerState()` | constructor |
| `Moss_GPUSamplerCreateInfo` | `new GpuSamplerCreateInfo(...)` | `new GpuSamplerCreateInfo()` | constructor |
| `Moss_GPUShaderCreateInfo` | `new GpuShaderCreateInfo(...)` | `new GpuShaderCreateInfo()` | constructor |
| `Moss_GPUStencilOpState` | `new GpuStencilOpState(...)` | `new GpuStencilOpState()` | constructor |
| `Moss_GPUTextureCreateInfo` | `new GpuTextureCreateInfo(...)` | `new GpuTextureCreateInfo()` | constructor |
| `Moss_GPUTextureLocation` | `new GpuTextureLocation(...)` | `new GpuTextureLocation()` | constructor |
| `Moss_GPUTextureRegion` | `new GpuTextureRegion(...)` | `new GpuTextureRegion()` | constructor |
| `Moss_GPUTextureSamplerBinding` | `new GpuTextureSamplerBinding(...)` | `new GpuTextureSamplerBinding()` | constructor |
| `Moss_GPUTextureTransferInfo` | `new GpuTextureTransferInfo(...)` | `new GpuTextureTransferInfo()` | constructor |
| `Moss_GPUTransferBufferCreateInfo` | `new GpuTransferBufferCreateInfo(...)` | `new GpuTransferBufferCreateInfo()` | constructor |
| `Moss_GPUTransferBufferLocation` | `new GpuTransferBufferLocation(...)` | `new GpuTransferBufferLocation()` | constructor |
| `Moss_GPUVertexAttribute` | `new GpuVertexAttribute(...)` | `new GpuVertexAttribute()` | constructor |
| `Moss_GPUVertexBufferDescription` | `new GpuVertexBufferDescription(...)` | `new GpuVertexBufferDescription()` | constructor |
| `Moss_GPUVertexInputState` | `new GpuVertexInputState(...)` | `new GpuVertexInputState()` | constructor |
| `Moss_GPUViewport` | `new GpuViewport(...)` | `new GpuViewport()` | constructor |
| `Moss_BindGPUComputeStorageBuffers` | `GpuCommandBuffer.bindComputeStorageBuffers(...)` | `GpuCommandBuffer.bindComputeStorageBuffers(int firstSlot, List<GpuStorageBufferReadWriteBinding> bindings) : void` | function |
| `Moss_BindGPUComputeStorageTextures` | `GpuCommandBuffer.bindComputeStorageTextures(...)` | `GpuCommandBuffer.bindComputeStorageTextures(int firstSlot, List<GpuStorageTextureReadWriteBinding> bindings) : void` | function |
| `Moss_BindGPUFragmentStorageBuffers` | `GpuCommandBuffer.bindFragmentStorageBuffers(...)` | `GpuCommandBuffer.bindFragmentStorageBuffers(int firstSlot, List<GpuStorageBufferReadWriteBinding> bindings) : void` | function |
| `Moss_BindGPUFragmentStorageTextures` | `GpuCommandBuffer.bindFragmentStorageTextures(...)` | `GpuCommandBuffer.bindFragmentStorageTextures(int firstSlot, List<GpuStorageTextureReadWriteBinding> bindings) : void` | function |
| `Moss_BindGPUVertexStorageBuffers` | `GpuCommandBuffer.bindVertexStorageBuffers(...)` | `GpuCommandBuffer.bindVertexStorageBuffers(int firstSlot, List<GpuStorageBufferReadWriteBinding> bindings) : void` | function |
| `Moss_BindGPUVertexStorageTextures` | `GpuCommandBuffer.bindVertexStorageTextures(...)` | `GpuCommandBuffer.bindVertexStorageTextures(int firstSlot, List<GpuStorageTextureReadWriteBinding> bindings) : void` | function |
| `Moss_GetGPUDeviceDriver` | `GpuDevice.getDriver(...)` | `GpuDevice.getDriver() : String` | function |
| `Moss_GetGPUDeviceProperties` | `GpuDevice.getProperties(...)` | `GpuDevice.getProperties() : GpuDeviceProperties` | function |
| `Moss_ReleaseWindowFromGPUDevice` | `GpuDevice.releaseWindowFrom(...)` | `GpuDevice.releaseWindowFrom(Window window) : void` | function |
| `Moss_ClaimWindowForGPUDevice` | `GpuDevice.claimWindowFor(...)` | `GpuDevice.claimWindowFor(Window window) : boolean` | function |
| `Moss_UpdateGPUResourceSet` | `ResourceSet.update(...)` | `ResourceSet.update(List<GpuResourceBinding> bindings) : void` | function |
| `Moss_CreateGPUDevice` | `GpuDevice.create(...)` | `GpuDevice.create(GpuDeviceDesc descriptor) : GpuDevice` | function |
| `Moss_CreateGPUDeviceWithProperties` | `GpuDevice.createWithProperties(...)` | `GpuDevice.createWithProperties(GpuDeviceDesc descriptor, GpuDeviceProperties requestedProperties) : GpuDevice` | function |
| `Moss_GPUBufferCreate` | `GpuDevice.createBuffer(...)` | `GpuDevice.createBuffer(GpuBufferDescriptor descriptor) : GpuBuffer` | function |
| `Moss_GPUBufferUpload` | `GpuBuffer.upload(...)` | `public void GpuBuffer.upload(ByteBuffer data, long offset)` | function |
| `Moss_GPUBufferMap` | `GpuBuffer.map(...)` | `GpuBuffer.map() : ByteBuffer` | function |
| `Moss_GPUBufferUnmap` | `GpuBuffer.unmap(...)` | `GpuBuffer.unmap() : void` | function |
| `Moss_GPUBufferDestroy` | `GpuBuffer.close(...)` | `GpuBuffer.close() : void` | function |
| `Moss_AcquireGPUSwapchainTexture` | `GpuTexture.acquireSwapchain(...)` | `GpuTexture.acquireSwapchain(Window window) : boolean` | function |
| `Moss_AcquireGPUSwapchainTextureStatus` | `GpuTexture.acquireSwapchainStatus(...)` | `GpuTexture.acquireSwapchainStatus(Window window) : GpuSwapchainResult` | function |
| `Moss_GetGPUSwapchainTextureFormat` | `GpuDevice.getSwapchainTextureFormat(...)` | `GpuDevice.getSwapchainTextureFormat(Window window) : TextureFormat` | function |
| `Moss_SetGPUSwapchainParameters` | `GpuDevice.setSwapchainParameters(...)` | `GpuDevice.setSwapchainParameters(Window window, int width, int height, GpuPresentMode presentMode) : boolean` | function |
| `Moss_ResizeGPUSwapchain` | `GpuDevice.resizeSwapchain(...)` | `GpuDevice.resizeSwapchain(Window window, int width, int height) : GpuSwapchainResult` | function |
| `Moss_WaitAndAcquireGPUSwapchainTexture` | `GpuTexture.waitAndAcquireSwapchain(...)` | `GpuTexture.waitAndAcquireSwapchain(Window window, long timeoutNs) : boolean` | function |
| `Moss_WaitAndAcquireGPUSwapchainTextureStatus` | `GpuTexture.waitAndAcquireSwapchainStatus(...)` | `GpuTexture.waitAndAcquireSwapchainStatus(Window window, long timeoutNs) : GpuSwapchainResult` | function |
| `Moss_WaitForGPUSwapchain` | `GpuDevice.waitForSwapchain(...)` | `GpuDevice.waitForSwapchain(Window window, long timeoutNs) : boolean` | function |
| `Moss_WindowSupportsGPUSwapchainComposition` | `Window.supportsSwapchainComposition(...)` | `Window.supportsSwapchainComposition() : boolean` | function |
| `Moss_PresentGPUSwapchain` | `GpuDevice.presentSwapchain(...)` | `GpuDevice.presentSwapchain(Window window, GpuTexture texture) : boolean` | function |
| `Moss_PresentGPUSwapchainStatus` | `GpuDevice.presentSwapchainStatus(...)` | `GpuDevice.presentSwapchainStatus(Window window, GpuTexture texture) : GpuSwapchainResult` | function |
| `Moss_GPUClear` | `GpuCommandBuffer.clear(...)` | `GpuCommandBuffer.clear(float r, float g, float b, float a) : void` | function |
| `Moss_GPUSamplerCreate` | `GpuDevice.createSampler(...)` | `GpuDevice.createSampler(GpuSamplerDesc desc) : GpuSampler` | function |
| `Moss_GPUSamplerDestroy` | `GpuSampler.close(...)` | `GpuSampler.close() : void` | function |
| `Moss_TextureCreate` | `GpuDevice.createTexture(...)` | `GpuDevice.createTexture(TextureDesc desc) : Texture` | function |
| `Moss_TextureDestroy` | `Texture.close(...)` | `Texture.close() : void` | function |
| `Moss_TextureUpload` | `Texture.upload(...)` | `Texture.upload(TextureUploadDesc desc) : void` | function |
| `Moss_TextureGenerateMips` | `Texture.generateMips(...)` | `Texture.generateMips() : void` | function |
| `Moss_GPUGetExternalFeatureMask` | `GPU.getExternalFeatureMask(...)` | `GPU.getExternalFeatureMask() : int` | function |
| `Moss_GPUExternalFeatureAvailable` | `GPU.externalFeatureAvailable(...)` | `GPU.externalFeatureAvailable(GpuExternalFeature feature) : boolean` | function |
| `Moss_GPUGetExternalFeatures` | `GPU.getExternalFeatures(...)` | `GPU.getExternalFeatures(GpuExternalFeatures outFeatures) : void` | function |
| `Moss_GPUExternalFeatureName` | `GPU.externalFeatureName(...)` | `GPU.externalFeatureName(GpuExternalFeature feature) : String` | function |
| `Moss_FSR1BuildEasuConstants` | `GPU.fsr1BuildEasuConstants(...)` | `GPU.fsr1BuildEasuConstants(int outConstants16, float inputViewportWidth, float inputViewportHeight, float inputWidth, float inputHeight, float outputWidth, float outputHeight) : void` | function |
| `Moss_FSR2GetUpscaleRatio` | `GPU.fsr2GetUpscaleRatio(...)` | `GPU.fsr2GetUpscaleRatio(FSR2QualityMode quality) : float` | function |
| `Moss_FSR2GetRenderResolution` | `GPU.fsr2GetRenderResolution(...)` | `GPU.fsr2GetRenderResolution(int displayWidth, int displayHeight, FSR2QualityMode quality, FSR2RenderResolution outResolution) : boolean` | function |
| `Moss_FSR2GetJitterPhaseCount` | `GPU.fsr2GetJitterPhaseCount(...)` | `GPU.fsr2GetJitterPhaseCount(int renderWidth, int displayWidth) : int` | function |
| `Moss_FSR2GetJitterOffset` | `GPU.fsr2GetJitterOffset(...)` | `GPU.fsr2GetJitterOffset(float[] outX, float[] outY, int index, int phaseCount) : boolean` | function |
| `Moss_MeshGenerateVertexRemap` | `GPU.meshGenerateVertexRemap(...)` | `GPU.meshGenerateVertexRemap(int[] destination, List<Uint32T> indices, ByteBuffer vertices, long vertexCount, long vertexSize) : long` | function |
| `Moss_MeshOptimizeVertexCache` | `GPU.meshOptimizeVertexCache(...)` | `GPU.meshOptimizeVertexCache(int[] destination, List<Uint32T> indices, long vertexCount) : boolean` | function |
| `Moss_MeshOptimizeOverdraw` | `GPU.meshOptimizeOverdraw(...)` | `GPU.meshOptimizeOverdraw(int[] destination, List<Uint32T> indices, List<Float> vertexPositions, long vertexPositionsStride, float threshold) : boolean` | function |
| `Moss_MeshOptimizeVertexFetch` | `GPU.meshOptimizeVertexFetch(...)` | `GPU.meshOptimizeVertexFetch(ByteBuffer destination, int[] indices, long indexCount, ByteBuffer vertices, long vertexCount, long vertexSize) : long` | function |
| `Moss_SMOLVEncode` | `GPU.smolvEncode(...)` | `GPU.smolvEncode(ByteBuffer spirvData, long spirvSize, ByteBuffer outSmolvData, long[] inoutSmolvSize, int flags) : boolean` | function |
| `Moss_SMOLVGetDecodedSize` | `GPU.smolvGetDecodedSize(...)` | `GPU.smolvGetDecodedSize(ByteBuffer smolvData, long smolvSize) : long` | function |
| `Moss_SMOLVDecode` | `GPU.smolvDecode(...)` | `GPU.smolvDecode(ByteBuffer smolvData, long smolvSize, ByteBuffer outSpirvData, long spirvSize, int flags) : boolean` | function |
| `Moss_GPUVulkanMemoryAllocatorAvailable` | `GPU.memoryAllocatorAvailable(...)` | `GPU.memoryAllocatorAvailable() : boolean` | function |
| `Moss_GPUClothSolverCreate` | `GpuDevice.createClothSolver(...)` | `GpuDevice.createClothSolver(GpuClothSolverDesc desc) : GpuClothSolver` | function |
| `Moss_GPUClothSolverDestroy` | `GpuClothSolver.close(...)` | `GpuClothSolver.close() : void` | function |
| `Moss_GPUClothSolverDispatch` | `GpuCommandBuffer.clothSolverDispatch(...)` | `GpuCommandBuffer.clothSolverDispatch(GpuClothSolver solver, GpuClothDispatchDesc desc) : boolean` | function |
| `Moss_GPUCommandBufferBegin` | `GpuDevice.createCommandBuffer(...)` | `GpuDevice.createCommandBuffer(CommandQueue queue) : GpuCommandBuffer` | function |
| `Moss_GPUCommandBufferEnd` | `GpuCommandBuffer.end(...)` | `GpuCommandBuffer.end() : void` | function |
| `Moss_GPUCommandBufferSubmit` | `GpuCommandBuffer.submit(...)` | `GpuCommandBuffer.submit() : void` | function |
| `Moss_CmdBeginRenderPass` | `GpuCommandBuffer.cmdBeginRenderPass(...)` | `GpuCommandBuffer.cmdBeginRenderPass(Framebuffer fb) : void` | function |
| `Moss_CmdEndRenderPass` | `GpuCommandBuffer.cmdEndRenderPass(...)` | `GpuCommandBuffer.cmdEndRenderPass() : void` | function |
| `Moss_CmdBindPipeline` | `GpuCommandBuffer.cmdBindPipeline(...)` | `GpuCommandBuffer.cmdBindPipeline(PipelineState pipeline) : void` | function |
| `Moss_CmdBindResourceSet` | `GpuCommandBuffer.cmdBindResourceSet(...)` | `GpuCommandBuffer.cmdBindResourceSet(int setIndex, ResourceSet set) : void` | function |
| `Moss_CmdBindVertexBuffer` | `GpuCommandBuffer.cmdBindVertex(...)` | `GpuCommandBuffer.cmdBindVertex(GpuBuffer buffer, long offset) : void` | function |
| `Moss_CmdBindIndexBuffer` | `GpuCommandBuffer.cmdBindIndex(...)` | `GpuCommandBuffer.cmdBindIndex(GpuBuffer buffer, long offset) : void` | function |
| `Moss_CmdDraw` | `GpuCommandBuffer.cmdDraw(...)` | `GpuCommandBuffer.cmdDraw(int vertexCount, int firstVertex) : void` | function |
| `Moss_CmdDrawIndexed` | `GpuCommandBuffer.cmdDrawIndexed(...)` | `GpuCommandBuffer.cmdDrawIndexed(int indexCount, int firstIndex, int vertexOffset) : void` | function |
| `Moss_CmdDispatch` | `GpuCommandBuffer.cmdDispatch(...)` | `GpuCommandBuffer.cmdDispatch(int x, int y, int z) : void` | function |
| `Moss_RGPassReadTexture` | `RGPass.readTexture(...)` | `RGPass.readTexture(RGTexture texture, ResourceState state) : RGTexture` | function |
| `Moss_RGPassWriteTexture` | `RGPass.writeTexture(...)` | `RGPass.writeTexture(RGTexture texture, ResourceState state) : RGTexture` | function |
| `Moss_GPUDeviceSupportsAsyncCompute` | `GpuDevice.supportsAsyncCompute(...)` | `GpuDevice.supportsAsyncCompute() : int` | function |
| `Moss_TextureSetResidency` | `Texture.setResidency(...)` | `Texture.setResidency(ResidencyState state) : void` | function |
| `Moss_ShaderCreate` | `GpuDevice.createShader(...)` | `GpuDevice.createShader(ShaderDesc desc) : Shader` | function |
| `Moss_ShaderDestroy` | `Shader.close(...)` | `Shader.close() : void` | function |
| `Moss_BindlessRegisterTexture` | `GpuDevice.bindlessRegisterTexture(...)` | `GpuDevice.bindlessRegisterTexture(ByteBuffer texture) : BindlessHandle` | function |
| `Moss_BindlessRegisterBuffer` | `GpuBuffer.bindlessRegister(...)` | `GpuBuffer.bindlessRegister() : BindlessHandle` | function |
| `Moss_BindlessUnregister` | `GpuDevice.bindlessUnregister(...)` | `GpuDevice.bindlessUnregister(BindlessHandle handle) : void` | function |
| `Moss_PipelineCreate` | `GpuDevice.createPipeline(...)` | `GpuDevice.createPipeline(PipelineDesc desc) : PipelineState` | function |
| `Moss_PipelineDestroy` | `GpuDevice.pipelineDestroy(...)` | `GpuDevice.pipelineDestroy(PipelineState pipeline) : void` | function |
| `Moss_WindowResizeVK` | `GPU.windowResizeVk(...)` | `GPU.windowResizeVk(VkPhysicalDevice physicalDevice, VkDevice device, VkSwapchainKHR swapchain, VkExtentDIMENSION2D swapchainExtent, StdVector<VkFramebuffer> swapchainFramebuffers, StdVector<VkImageView> swapchainImageViews, VkImage depthImage, VkDeviceMemory depthImageMemory, VkImageView depthImageView) : void` | function |
| `Moss_GPUFatalError` | `GPU.fatalError(...)` | `GPU.fatalError(MossResult result) : void` | function |
| `Moss_SetGPUViewports` | `GpuCommandBuffer.setViewports(...)` | `GpuCommandBuffer.setViewports(List<GpuViewport> viewports) : void` | function |
| `Moss_SetGPUScissors` | `GpuCommandBuffer.setScissors(...)` | `GpuCommandBuffer.setScissors(List<Rect> scissors) : void` | function |
| `Moss_ClearGPUTexture` | `GpuCommandBuffer.clearTexture(...)` | `GpuCommandBuffer.clearTexture(GpuTexture texture, float color) : void` | function |
| `Moss_ClearGPUDepthStencilTexture` | `GpuCommandBuffer.clearDepthStencilTexture(...)` | `GpuCommandBuffer.clearDepthStencilTexture(GpuTexture texture, float depth, byte stencil) : void` | function |
| `Moss_DrawGPUIndexedPrimitivesIndirectCount` | `GpuCommandBuffer.drawIndexedPrimitivesIndirectCount(...)` | `GpuCommandBuffer.drawIndexedPrimitivesIndirectCount(GpuBuffer indirectBuffer, long offset, GpuBuffer countBuffer, long countOffset, int maxDrawCount, int stride) : void` | function |
| `Moss_DrawGPUPrimitivesIndirectCount` | `GpuCommandBuffer.drawPrimitivesIndirectCount(...)` | `GpuCommandBuffer.drawPrimitivesIndirectCount(GpuBuffer indirectBuffer, long offset, GpuBuffer countBuffer, long countOffset, int maxDrawCount, int stride) : void` | function |
| `Moss_SetGPUDepthBias` | `GpuCommandBuffer.setDepthBias(...)` | `GpuCommandBuffer.setDepthBias(float constantFactor, float clamp, float slopeFactor) : void` | function |
| `Moss_CreateGPUSemaphore` | `GpuDevice.createSemaphore(...)` | `GpuDevice.createSemaphore() : GpuSemaphore` | function |
| `Moss_ReleaseGPUSemaphore` | `GpuSemaphore.release(...)` | `GpuSemaphore.release() : void` | function |
| `Moss_SignalGPUSemaphoreValue` | `GpuSemaphore.signalValue(...)` | `GpuSemaphore.signalValue(long value) : void` | function |
| `Moss_WaitGPUSemaphoreValue` | `GpuSemaphore.waitValue(...)` | `GpuSemaphore.waitValue(long value, long timeoutNs) : boolean` | function |
| `Moss_SubmitGPUCommandBufferWithSemaphores` | `GpuCommandBuffer.submitWithSemaphores(...)` | `GpuCommandBuffer.submitWithSemaphores(GpuSemaphore waitSemaphores, List<Uint64T> waitValues, GpuSemaphore signalSemaphores, List<Uint64T> signalValues) : void` | function |
| `Moss_GetGPUBindlessCapacity` | `GpuDevice.getBindlessCapacity(...)` | `GpuDevice.getBindlessCapacity(ShaderResourceType type) : int` | function |
| `Moss_CreateGPUPipelineCache` | `GpuDevice.createPipelineCache(...)` | `GpuDevice.createPipelineCache(ByteBuffer initialData, long initialSize) : GpuPipelineCache` | function |
| `Moss_GetGPUPipelineCacheData` | `GpuPipelineCache.getData(...)` | `GpuPipelineCache.getData(ByteBuffer outData, long outSize) : long` | function |
| `Moss_ReleaseGPUPipelineCache` | `GpuPipelineCache.release(...)` | `GpuPipelineCache.release() : void` | function |

### `Moss_GUI.h` → `moss.gui`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `CreateContext` | `GUI.createContext(...)` | `GUI.createContext(FontAtlas sharedFontAtlas) : GuiContext` | function |
| `DestroyContext` | `GuiContext.close(...)` | `GuiContext.close() : void` | function |
| `GetCurrentContext` | `GUI.getCurrentContext(...)` | `GUI.getCurrentContext() : GuiContext` | function |
| `SetCurrentContext` | `GuiContext.setCurrent(...)` | `GuiContext.setCurrent() : void` | function |
| `BeginWindow` | `GuiContext.beginWindow(...)` | `GuiContext.beginWindow(String name, boolean[] pOpen, GuiWindowFlags flags) : boolean` | function |
| `EndWindow` | `GuiContext.endWindow(...)` | `GuiContext.endWindow() : void` | function |
| `BeginWindowChild` (overload 1) | `GuiContext.beginWindowChild(...)` | `GuiContext.beginWindowChild(String strId, Float2 size, GuiChildFlags childFlags, GuiWindowFlags windowFlags) : boolean` | function |
| `BeginWindowChild` (overload 2) | `GuiContext.beginWindowChild(...)` | `GuiContext.beginWindowChild(int id, Float2 size, GuiChildFlags childFlags, GuiWindowFlags windowFlags) : boolean` | function |
| `EndWindowChild` | `GuiContext.endWindowChild(...)` | `GuiContext.endWindowChild() : void` | function |
| `BeginContainer` | `GuiContext.beginContainer(...)` | `GuiContext.beginContainer(Rect rect) : void` | function |
| `EndContainer` | `GuiContext.endContainer(...)` | `GuiContext.endContainer() : void` | function |
| `BeginVContainer` | `GuiContext.beginVContainer(...)` | `GuiContext.beginVContainer(Rect rect, float spacing) : void` | function |
| `EndVContainer` | `GuiContext.endVContainer(...)` | `GuiContext.endVContainer() : void` | function |
| `BeginHContainer` | `GuiContext.beginHContainer(...)` | `GuiContext.beginHContainer(Rect rect, float spacing) : void` | function |
| `EndHContainer` | `GuiContext.endHContainer(...)` | `GuiContext.endHContainer() : void` | function |
| `BeginCenterContainer` | `GuiContext.beginCenterContainer(...)` | `GuiContext.beginCenterContainer(Rect rect) : void` | function |
| `EndCenterContainer` | `GuiContext.endCenterContainer(...)` | `GuiContext.endCenterContainer() : void` | function |
| `BeginMarginContainer` | `GuiContext.beginMarginContainer(...)` | `GuiContext.beginMarginContainer(float leftMargin, float rightMargin, float topMargin, float bottomMargin) : void` | function |
| `EndMarginContainer` | `GuiContext.endMarginContainer(...)` | `GuiContext.endMarginContainer() : void` | function |
| `BeginVScrollContainer` | `GuiContext.beginVScrollContainer(...)` | `GuiContext.beginVScrollContainer() : void` | function |
| `EndVScrollContainer` | `GuiContext.endVScrollContainer(...)` | `GuiContext.endVScrollContainer() : void` | function |
| `BeginHScrollContainer` | `GuiContext.beginHScrollContainer(...)` | `GuiContext.beginHScrollContainer() : void` | function |
| `EndHScrollContainer` | `GuiContext.endHScrollContainer(...)` | `GuiContext.endHScrollContainer() : void` | function |
| `BeginGridContainer` | `GuiContext.beginGridContainer(...)` | `GuiContext.beginGridContainer() : void` | function |
| `EndGridContainer` | `GuiContext.endGridContainer(...)` | `GuiContext.endGridContainer() : void` | function |
| `ColorRect` | `GuiContext.colorRect(...)` | `GuiContext.colorRect(Color color, Float2 size) : void` | function |
| `Text` | `GuiContext.text(...)` | `GuiContext.text(String fmt, ... args) : void` | function |
| `RichText` | `GuiContext.richText(...)` | `GuiContext.richText(String label) : void` | function |
| `ProgressBar` | `GuiContext.progressBar(...)` | `GuiContext.progressBar() : void` | function |
| `TextLink` | `GuiContext.textLink(...)` | `GuiContext.textLink(String label) : boolean` | function |
| `TextLinkOpenURL` | `GuiContext.textLinkOpenUrl(...)` | `GuiContext.textLinkOpenUrl(String label, String url) : void` | function |
| `Button` | `GuiContext.button(...)` | `GuiContext.button(String label, Rect rectPtr, Mat44 vpMatrix, float defaultWidth, float defaultHeight) : boolean` | function |
| `ToggleSwitch` | `GuiContext.toggleSwitch(...)` | `GuiContext.toggleSwitch(boolean active) : boolean` | function |
| `RadioButton` | `GuiContext.radioButton(...)` | `GuiContext.radioButton(String label, boolean active) : boolean` | function |
| `Checkbox` | `GuiContext.checkbox(...)` | `GuiContext.checkbox(String label, boolean[] value, Rect rectPtr, Mat44 vpMatrix, float defaultSize) : boolean` | function |
| `SliderFloat` | `GuiContext.sliderFloat(...)` | `GuiContext.sliderFloat(float minValue, float maxValue, float value) : void` | function |
| `SliderDouble` | `GuiContext.sliderDouble(...)` | `GuiContext.sliderDouble(double minValue, double maxValue, double value) : void` | function |
| `SliderInt` | `GuiContext.sliderInt(...)` | `GuiContext.sliderInt(int minValue, int maxValue, int value) : void` | function |
| `SliderFloat2` | `GuiContext.sliderFloat2(...)` | `GuiContext.sliderFloat2(Float2 minValue, Float2 maxValue, Float2 value) : void` | function |
| `SliderDouble2` | `GuiContext.sliderDouble2(...)` | `GuiContext.sliderDouble2(Double2 minValue, Double2 maxValue, Double2 value) : void` | function |
| `SliderInt2` | `GuiContext.sliderInt2(...)` | `GuiContext.sliderInt2(Int2 minValue, Int2 maxValue, Int2 value) : void` | function |
| `SliderFloat3` | `GuiContext.sliderFloat3(...)` | `GuiContext.sliderFloat3(Float3 minValue, Float3 maxValue, Float3 value) : void` | function |
| `SliderDouble3` | `GuiContext.sliderDouble3(...)` | `GuiContext.sliderDouble3(Double3 minValue, Double3 maxValue, Double3 value) : void` | function |
| `SliderInt3` | `GuiContext.sliderInt3(...)` | `GuiContext.sliderInt3(Int3 minValue, Int3 maxValue, Int3 value) : void` | function |
| `SliderFloat4` | `GuiContext.sliderFloat4(...)` | `GuiContext.sliderFloat4(Float4 minValue, Float4 maxValue, Float4 value) : void` | function |
| `SliderDouble4` | `GuiContext.sliderDouble4(...)` | `GuiContext.sliderDouble4(Double4 minValue, Double4 maxValue, Double4 value) : void` | function |
| `SliderInt4` | `GuiContext.sliderInt4(...)` | `GuiContext.sliderInt4(Int4 minValue, Int4 maxValue, Int4 value) : void` | function |
| `DragFloat` | `GuiContext.dragFloat(...)` | `GuiContext.dragFloat(String label, float[] value, float vSpeed, float vMin, float vMax, String format, GuiSliderFlags flags) : boolean` | function |
| `DragFloat2` | `GuiContext.dragFloat2(...)` | `GuiContext.dragFloat2(String label, Float2 value, float vSpeed, float vMin, float vMax, String format, GuiSliderFlags flags) : boolean` | function |
| `DragFloat3` | `GuiContext.dragFloat3(...)` | `GuiContext.dragFloat3(String label, Float3 value, float vSpeed, float vMin, float vMax, String format, GuiSliderFlags flags) : boolean` | function |
| `DragFloat4` | `GuiContext.dragFloat4(...)` | `GuiContext.dragFloat4(String label, Float4 value, float vSpeed, float vMin, float vMax, String format, GuiSliderFlags flags) : boolean` | function |
| `DragFloatRange2` | `GuiContext.dragFloatRange2(...)` | `GuiContext.dragFloatRange2(String label, float[] vCurrentMin, float[] vCurrentMax, float vSpeed, float vMin, float vMax, String format, String formatMax, GuiSliderFlags flags) : boolean` | function |
| `DragInt` | `GuiContext.dragInt(...)` | `GuiContext.dragInt(String label, int[] v, float vSpeed, int vMin, int vMax, String format, GuiSliderFlags flags) : boolean` | function |
| `DragInt2` | `GuiContext.dragInt2(...)` | `GuiContext.dragInt2(String label, int v, float vSpeed, int vMin, int vMax, String format, GuiSliderFlags flags) : boolean` | function |
| `DragInt3` | `GuiContext.dragInt3(...)` | `GuiContext.dragInt3(String label, int v, float vSpeed, int vMin, int vMax, String format, GuiSliderFlags flags) : boolean` | function |
| `DragInt4` | `GuiContext.dragInt4(...)` | `GuiContext.dragInt4(String label, int v, float vSpeed, int vMin, int vMax, String format, GuiSliderFlags flags) : boolean` | function |
| `DragIntRange2` | `GuiContext.dragIntRange2(...)` | `GuiContext.dragIntRange2(String label, int[] vCurrentMin, int[] vCurrentMax, float vSpeed, int vMin, int vMax, String format, String formatMax, GuiSliderFlags flags) : boolean` | function |
| `DragScalar` | `GuiContext.dragScalar(...)` | `GuiContext.dragScalar(String label, GuiDataType dataType, ByteBuffer pData, float vSpeed, ByteBuffer pMin, ByteBuffer pMax, String format, GuiSliderFlags flags) : boolean` | function |
| `DragScalarN` | `GuiContext.dragScalarN(...)` | `GuiContext.dragScalarN(String label, GuiDataType dataType, ByteBuffer pData, int components, float vSpeed, ByteBuffer pMin, ByteBuffer pMax, String format, GuiSliderFlags flags) : boolean` | function |
| `InputText` | `GuiContext.inputText(...)` | `GuiContext.inputText(String label, ByteBuffer buf, long bufSize, GuiInputTextFlags flags, GuiInputTextCallback callback) : boolean` | function |
| `InputTextMultiline` | `GuiContext.inputTextMultiline(...)` | `GuiContext.inputTextMultiline(String label, ByteBuffer buf, long bufSize, Float2 size, GuiInputTextFlags flags, GuiInputTextCallback callback) : boolean` | function |
| `InputTextWithHint` | `GuiContext.inputTextWithHint(...)` | `GuiContext.inputTextWithHint(String label, String hint, ByteBuffer buf, long bufSize, GuiInputTextFlags flags, GuiInputTextCallback callback) : boolean` | function |
| `InputFloat` | `GuiContext.inputFloat(...)` | `GuiContext.inputFloat(String label, float[] v, float step, float stepFast, String format, GuiInputTextFlags flags) : boolean` | function |
| `InputFloat2` | `GuiContext.inputFloat2(...)` | `GuiContext.inputFloat2(String label, float v, String format, GuiInputTextFlags flags) : boolean` | function |
| `InputFloat3` | `GuiContext.inputFloat3(...)` | `GuiContext.inputFloat3(String label, float v, String format, GuiInputTextFlags flags) : boolean` | function |
| `InputFloat4` | `GuiContext.inputFloat4(...)` | `GuiContext.inputFloat4(String label, float v, String format, GuiInputTextFlags flags) : boolean` | function |
| `InputInt` | `GuiContext.inputInt(...)` | `GuiContext.inputInt(String label, int[] v, int step, int stepFast, GuiInputTextFlags flags) : boolean` | function |
| `InputInt2` | `GuiContext.inputInt2(...)` | `GuiContext.inputInt2(String label, int v, GuiInputTextFlags flags) : boolean` | function |
| `InputInt3` | `GuiContext.inputInt3(...)` | `GuiContext.inputInt3(String label, int v, GuiInputTextFlags flags) : boolean` | function |
| `InputInt4` | `GuiContext.inputInt4(...)` | `GuiContext.inputInt4(String label, int v, GuiInputTextFlags flags) : boolean` | function |
| `InputDouble` | `GuiContext.inputDouble(...)` | `GuiContext.inputDouble(String label, double[] v, double step, double stepFast, String format, GuiInputTextFlags flags) : boolean` | function |
| `InputScalar` | `GuiContext.inputScalar(...)` | `GuiContext.inputScalar(String label, GuiDataType dataType, ByteBuffer pData, ByteBuffer pStep, ByteBuffer pStepFast, String format, GuiInputTextFlags flags) : boolean` | function |
| `InputScalarN` | `GuiContext.inputScalarN(...)` | `GuiContext.inputScalarN(String label, GuiDataType dataType, ByteBuffer pData, int components, ByteBuffer pStep, ByteBuffer pStepFast, String format, GuiInputTextFlags flags) : boolean` | function |

### `Moss_Network.h` → `moss.network`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_Init_Network` | `Network.initialize(...)` | `Network.initialize() : void` | function |
| `enet_initialize_with_callbacks` | `NetworkCallbacks.initializeWith(...)` | `NetworkCallbacks.initializeWith() : int` | function |
| `Moss_TerminateNetwork` | `Network.shutdown(...)` | `Network.shutdown() : void` | function |
| `enet_list_clear` | `NetworkList.clear(...)` | `NetworkList.clear() : void` | function |
| `enet_list_insert` | `Network.listInsert(...)` | `Network.listInsert(E netListIterator, ByteBuffer value) : NetworkListIterator` | function |
| `enet_list_remove` | `Network.listRemove(...)` | `Network.listRemove(E netListIterator) : ByteBuffer` | function |
| `enet_list_move` | `Network.listMove(...)` | `Network.listMove(E netListIterator, ByteBuffer value, ByteBuffer value) : NetworkListIterator` | function |
| `enet_list_size` | `NetworkList.size(...)` | `NetworkList.size() : long` | function |
| `enet_time_get` | `Network.timeGet(...)` | `Network.timeGet() : int` | function |
| `enet_time_set` | `Network.timeSet(...)` | `Network.timeSet(U int32T) : void` | function |
| `enet_socket_create` | `Network.createSocket(...)` | `Network.createSocket(NetworkAddressType adressType, NetworkSocketType socketType) : NetworkSocket` | function |
| `enet_socket_bind` | `NetworkAddress.socketBind(...)` | `NetworkAddress.socketBind(E netSocket) : int` | function |
| `enet_socket_get_address` | `NetworkAddress.socketGet(...)` | `NetworkAddress.socketGet(E netSocket) : int` | function |
| `enet_socket_listen` | `Network.socketListen(...)` | `Network.socketListen(E netSocket, I nt) : int` | function |
| `enet_socket_accept` | `NetworkAddress.socketAccept(...)` | `NetworkAddress.socketAccept(E netSocket) : NetworkSocket` | function |
| `enet_socket_connect` | `NetworkAddress.socketConnect(...)` | `NetworkAddress.socketConnect(E netSocket) : int` | function |
| `enet_socket_send` | `NetworkAddress.socketSend(...)` | `NetworkAddress.socketSend(E netSocket, NetworkBuffer value, S izeT) : int` | function |
| `enet_socket_receive` | `NetworkAddress.socketReceive(...)` | `NetworkAddress.socketReceive(E netSocket, NetworkBuffer value, S izeT) : int` | function |
| `enet_socket_wait` | `Network.socketWait(...)` | `Network.socketWait(E netSocket, int[] value, U int32T) : int` | function |
| `enet_socket_set_option` | `Network.socketSetOption(...)` | `Network.socketSetOption(E netSocket, E netSocketOption, I nt) : int` | function |
| `enet_socket_get_option` | `Network.socketGetOption(...)` | `Network.socketGetOption(E netSocket, E netSocketOption, int[] value) : int` | function |
| `enet_socket_shutdown` | `Network.socketShutdown(...)` | `Network.socketShutdown(E netSocket, E netSocketShutdown) : int` | function |
| `enet_socket_destroy` | `Network.socketDestroy(...)` | `Network.socketDestroy(E netSocket) : void` | function |
| `enet_socketset_select` | `NetworkSocketSet.socketsetSelect(...)` | `NetworkSocketSet.socketsetSelect(E netSocket, NetworkSocketSet value, U int32T) : int` | function |
| `enet_address_set_host_ip` | `NetworkAddress.setHostIp(...)` | `NetworkAddress.setHostIp(String hostName) : int` | function |
| `enet_address_set_host` | `NetworkAddress.setHost(...)` | `NetworkAddress.setHost(NetworkAddressType type, String hostName) : int` | function |
| `enet_address_get_host_ip` | `NetworkAddress.getHostIp(...)` | `NetworkAddress.getHostIp(ByteBuffer hostName, long nameLength) : int` | function |
| `enet_address_get_host` | `NetworkAddress.getHost(...)` | `NetworkAddress.getHost(ByteBuffer hostName, long nameLength) : int` | function |
| `enet_address_equal_host` | `NetworkAddress.equalHost(...)` | `NetworkAddress.equalHost(NetworkAddress secondAddress) : int` | function |
| `enet_address_is_broadcast` | `NetworkAddress.isBroadcast(...)` | `NetworkAddress.isBroadcast() : int` | function |
| `enet_address_build_any` | `NetworkAddress.buildAny(...)` | `NetworkAddress.buildAny(NetworkAddressType type) : void` | function |
| `enet_address_build_loopback` | `NetworkAddress.buildLoopback(...)` | `NetworkAddress.buildLoopback(NetworkAddressType type) : void` | function |
| `enet_address_convert_ipv6` | `NetworkAddress.convertIpv6(...)` | `NetworkAddress.convertIpv6() : void` | function |
| `enet_crc32` | `NetworkBuffer.crc32(...)` | `NetworkBuffer.crc32(S izeT) : int` | function |
| `enet_host_create` | `NetworkAddress.createHost(...)` | `NetworkAddress.createHost(NetworkAddressType type, S izeT, S izeT, U int32T, U int32T) : NetworkHost` | function |
| `enet_host_destroy` | `NetworkHost.close(...)` | `NetworkHost.close() : void` | function |
| `enet_host_check_events` | `NetworkHost.checkEvents(...)` | `NetworkHost.checkEvents(NetworkEvent value) : int` | function |
| `enet_host_service` | `NetworkHost.service(...)` | `NetworkHost.service(NetworkEvent value, U int32T) : int` | function |
| `enet_host_flush` (overload 1) | `NetworkHost.flush(...)` | `NetworkHost.flush() : void` | function |
| `enet_host_broadcast` (overload 1) | `NetworkHost.broadcast(...)` | `NetworkHost.broadcast(U int8T, NetworkPacket value) : void` | function |
| `enet_host_compress` | `NetworkHost.compress(...)` | `NetworkHost.compress(NetworkCompressor value) : void` | function |
| `enet_host_compress_with_range_coder` | `NetworkHost.compressWithRangeCoder(...)` | `NetworkHost.compressWithRangeCoder() : int` | function |
| `enet_host_channel_limit` | `NetworkHost.channelLimit(...)` | `NetworkHost.channelLimit(S izeT) : void` | function |
| `enet_host_bandwidth_limit` | `NetworkHost.bandwidthLimit(...)` | `NetworkHost.bandwidthLimit(U int32T, U int32T) : void` | function |
| `enet_host_bandwidth_throttle` | `NetworkHost.bandwidthThrottle(...)` | `NetworkHost.bandwidthThrottle() : void` | function |
| `enet_host_random_seed` | `Network.hostRandomSeed(...)` | `Network.hostRandomSeed() : int` | function |
| `enet_host_random` | `NetworkHost.random(...)` | `NetworkHost.random() : int` | function |
| `enet_host_connect` (overload 1) | `NetworkHost.connect(...)` | `NetworkHost.connect(NetworkAddress value, S izeT, U int32T) : NetworkPeer` | function |
| `enet_peer_send` | `NetworkPeer.send(...)` | `NetworkPeer.send(U int8T, NetworkPacket value) : int` | function |
| `enet_peer_ping` (overload 1) | `NetworkPeer.ping(...)` | `NetworkPeer.ping() : void` | function |
| `enet_peer_ping_interval` | `NetworkPeer.pingInterval(...)` | `NetworkPeer.pingInterval(U int32T) : void` | function |
| `enet_peer_timeout` (overload 1) | `NetworkPeer.timeout(...)` | `NetworkPeer.timeout(U int32T, U int32T, U int32T) : void` | function |
| `enet_peer_reset` (overload 1) | `NetworkPeer.reset(...)` | `NetworkPeer.reset() : void` | function |
| `enet_peer_disconnect` (overload 1) | `NetworkPeer.disconnect(...)` | `NetworkPeer.disconnect(U int32T) : void` | function |
| `enet_peer_disconnect_now` (overload 1) | `NetworkPeer.disconnectNow(...)` | `NetworkPeer.disconnectNow(U int32T) : void` | function |
| `enet_peer_disconnect_later` | `NetworkPeer.disconnectLater(...)` | `NetworkPeer.disconnectLater(U int32T) : void` | function |
| `enet_peer_throttle_configure` | `NetworkPeer.throttleConfigure(...)` | `NetworkPeer.throttleConfigure(U int32T, U int32T, U int32T) : void` | function |
| `enet_peer_throttle` | `NetworkPeer.throttle(...)` | `NetworkPeer.throttle(U int32T) : int` | function |
| `enet_peer_reset_queues` | `NetworkPeer.resetQueues(...)` | `NetworkPeer.resetQueues() : void` | function |
| `enet_peer_has_outgoing_commands` | `NetworkPeer.hasOutgoingCommands(...)` | `NetworkPeer.hasOutgoingCommands() : int` | function |
| `enet_peer_setup_outgoing_command` | `NetworkOutgoingCommand.peerSetup(...)` | `NetworkOutgoingCommand.peerSetup(NetworkPeer value) : void` | function |
| `enet_peer_queue_outgoing_command` | `NetworkPeer.queueOutgoingCommand(...)` | `NetworkPeer.queueOutgoingCommand(NetworkProtocol value, NetworkPacket value, U int32T, U int16T) : NetworkOutgoingCommand` | function |
| `enet_peer_queue_incoming_command` | `NetworkPeer.queueIncomingCommand(...)` | `NetworkPeer.queueIncomingCommand(NetworkProtocol value, ByteBuffer value, S izeT, U int32T, U int32T) : NetworkIncomingCommand` | function |
| `enet_peer_queue_acknowledgement` | `NetworkPeer.queueAcknowledgement(...)` | `NetworkPeer.queueAcknowledgement(NetworkProtocol value, U int16T) : NetworkAcknowledgement` | function |
| `enet_peer_dispatch_incoming_unreliable_commands` | `NetworkIncomingCommand.peerDispatchIncomingUnreliableCommands(...)` | `NetworkIncomingCommand.peerDispatchIncomingUnreliableCommands(NetworkPeer value, NetworkChannel value) : void` | function |
| `enet_peer_dispatch_incoming_reliable_commands` | `NetworkIncomingCommand.peerDispatchIncomingReliableCommands(...)` | `NetworkIncomingCommand.peerDispatchIncomingReliableCommands(NetworkPeer value, NetworkChannel value) : void` | function |
| `enet_peer_on_connect` | `NetworkPeer.onConnect(...)` | `NetworkPeer.onConnect() : void` | function |
| `enet_peer_on_disconnect` | `NetworkPeer.onDisconnect(...)` | `NetworkPeer.onDisconnect() : void` | function |
| `enet_packet_create` | `Network.createPacket(...)` | `Network.createPacket(ByteBuffer value, S izeT, U int32T) : NetworkPacket` | function |
| `enet_packet_destroy` | `NetworkPacket.close(...)` | `NetworkPacket.close() : void` | function |
| `enet_packet_resize` | `NetworkPacket.resize(...)` | `NetworkPacket.resize(S izeT) : int` | function |
| `enet_peer_receive` | `NetworkPeer.receive(...)` | `NetworkPeer.receive(byte[] channelId) : NetworkPacket` | function |
| `enet_range_coder_create` | `Network.createRangeCoder(...)` | `Network.createRangeCoder() : ByteBuffer` | function |
| `enet_range_coder_destroy` | `Network.rangeCoderDestroy(...)` | `Network.rangeCoderDestroy(ByteBuffer value) : void` | function |
| `enet_range_coder_compress` | `NetworkBuffer.rangeCoderCompress(...)` | `NetworkBuffer.rangeCoderCompress(ByteBuffer value, S izeT, S izeT, byte[] value, S izeT) : long` | function |
| `enet_range_coder_decompress` | `Network.rangeCoderDecompress(...)` | `Network.rangeCoderDecompress(ByteBuffer value, byte[] value, S izeT, byte[] value, S izeT) : long` | function |
| `enet_protocol_command_size` | `Network.protocolCommandSize(...)` | `Network.protocolCommandSize(U int8T) : long` | function |
| `enet_packet_get_data` | `NetworkPacket.getData(...)` | `NetworkPacket.getData() : ByteBuffer` | function |
| `enet_packet_get_user_data` | `NetworkPacket.getUserData(...)` | `NetworkPacket.getUserData() : ByteBuffer` | function |
| `enet_packet_set_user_data` | `NetworkPacket.setUserData(...)` | `NetworkPacket.setUserData() : void` | function |
| `enet_packet_get_length` | `NetworkPacket.getLength(...)` | `NetworkPacket.getLength() : int` | function |
| `enet_packet_set_acknowledge_callback` | `NetworkPacket.setAcknowledgeCallback(...)` | `NetworkPacket.setAcknowledgeCallback(E netPacketAcknowledgedCallback) : void` | function |
| `enet_packet_set_free_callback` | `NetworkPacket.setFreeCallback(...)` | `NetworkPacket.setFreeCallback(E netPacketFreeCallback) : void` | function |
| `enet_packet_check_references` | `NetworkPacket.checkReferences(...)` | `NetworkPacket.checkReferences() : int` | function |
| `enet_packet_dispose` | `NetworkPacket.dispose(...)` | `NetworkPacket.dispose() : void` | function |
| `enet_host_get_peers_count` | `NetworkHost.getPeersCount(...)` | `NetworkHost.getPeersCount() : int` | function |
| `enet_host_get_packets_sent` | `NetworkHost.getPacketsSent(...)` | `NetworkHost.getPacketsSent() : int` | function |
| `enet_host_get_packets_received` | `NetworkHost.getPacketsReceived(...)` | `NetworkHost.getPacketsReceived() : int` | function |
| `enet_host_get_bytes_sent` | `NetworkHost.getBytesSent(...)` | `NetworkHost.getBytesSent() : int` | function |
| `enet_host_get_bytes_received` | `NetworkHost.getBytesReceived(...)` | `NetworkHost.getBytesReceived() : int` | function |
| `enet_host_set_max_duplicate_peers` | `NetworkHost.setMaxDuplicatePeers(...)` | `NetworkHost.setMaxDuplicatePeers(U int16T) : void` | function |
| `enet_host_set_intercept_callback` | `NetworkHost.setInterceptCallback(...)` | `NetworkHost.setInterceptCallback(E netInterceptCallback) : void` | function |
| `enet_host_set_checksum_callback` | `NetworkHost.setChecksumCallback(...)` | `NetworkHost.setChecksumCallback(E netChecksumCallback) : void` | function |
| `enet_peer_get_id` | `NetworkPeer.getId(...)` | `NetworkPeer.getId() : int` | function |
| `enet_peer_get_ip` | `NetworkPeer.getIp(...)` | `NetworkPeer.getIp(ByteBuffer value, S izeT) : int` | function |
| `enet_peer_get_port` | `NetworkPeer.getPort(...)` | `NetworkPeer.getPort() : int` | function |
| `enet_peer_get_mtu` | `NetworkPeer.getMtu(...)` | `NetworkPeer.getMtu() : int` | function |
| `enet_peer_get_state` | `NetworkPeer.getState(...)` | `NetworkPeer.getState() : NetworkPeerState` | function |
| `enet_peer_get_rtt` | `NetworkPeer.getRtt(...)` | `NetworkPeer.getRtt() : int` | function |
| `enet_peer_get_last_rtt` | `NetworkPeer.getLastRtt(...)` | `NetworkPeer.getLastRtt() : int` | function |
| `enet_peer_get_lastsendtime` | `NetworkPeer.getLastsendtime(...)` | `NetworkPeer.getLastsendtime() : int` | function |
| `enet_peer_get_lastreceivetime` | `NetworkPeer.getLastreceivetime(...)` | `NetworkPeer.getLastreceivetime() : int` | function |
| `enet_peer_get_packets_throttle` | `NetworkPeer.getPacketsThrottle(...)` | `NetworkPeer.getPacketsThrottle() : float` | function |
| `enet_peer_get_data` | `NetworkPeer.getData(...)` | `NetworkPeer.getData() : ByteBuffer` | function |
| `enet_peer_set_data` | `NetworkPeer.setData(...)` | `NetworkPeer.setData(ByteBuffer value) : void` | function |
| `enet_host_create_with_transport` (overload 1) | `NetworkAddress.hostCreateWithTransport(...)` | `NetworkAddress.hostCreateWithTransport(long peerCount, long channelLimit, int incomingBandwidth, int outgoingBandwidth, NetworkTransportType transport) : NetworkHost` | function |
| `enet_host_connect` (overload 2) | `NetworkHost.connect(...)` | `NetworkHost.connect(NetworkAddress address, long channelCount, int data) : NetworkPeer` | function |
| `enet_host_connect_ex` (overload 1) | `NetworkHost.connectEx(...)` | `NetworkHost.connectEx(String endpoint, long channelCount, int data) : NetworkPeer` | function |
| `enet_host_transport_caps` (overload 1) | `NetworkHost.transportCaps(...)` | `NetworkHost.transportCaps() : int` | function |
| `enet_transport_handle_event` | `NetworkTransportEvent.transportHandle(...)` | `NetworkTransportEvent.transportHandle(NetworkHost host) : void` | function |
| `enet_peer_reset` (overload 2) | `NetworkPeer.reset(...)` | `NetworkPeer.reset() : void` | function |
| `enet_peer_timeout` (overload 2) | `NetworkPeer.timeout(...)` | `NetworkPeer.timeout() : void` | function |
| `enet_peer_ping` (overload 2) | `NetworkPeer.ping(...)` | `NetworkPeer.ping() : void` | function |
| `enet_packet_encode` | `NetPacket.encode(...)` | `NetPacket.encode(Uint8 outBuffer, long bufferSize) : long` | function |
| `enet_packet_decode` | `Uint8.packetDecode(...)` | `Uint8.packetDecode(NetPacket outPacket, long bufferSize) : boolean` | function |
| `enet_fragment_send` | `NetworkPeer.fragmentSend(...)` | `NetworkPeer.fragmentSend(Uint8 channelId, List<Uint8> data) : void` | function |
| `enet_fragment_receive` | `NetworkPeer.fragmentReceive(...)` | `NetworkPeer.fragmentReceive(NetPacket fragment, NetPacket outPacket) : boolean` | function |
| `enet_bandwidth_update` | `NetworkHost.bandwidthUpdate(...)` | `NetworkHost.bandwidthUpdate(float deltaTime) : void` | function |
| `enet_peer_send_window` | `NetworkPeer.sendWindow(...)` | `NetworkPeer.sendWindow() : long` | function |
| `enet_peer_create_channel` | `NetworkPeer.createChannel(...)` | `NetworkPeer.createChannel(boolean reliable, boolean ordered) : int` | function |
| `enet_peer_reset_channels` | `NetworkPeer.resetChannels(...)` | `NetworkPeer.resetChannels() : void` | function |
| `enet_peer_disconnect` (overload 2) | `NetworkPeer.disconnect(...)` | `NetworkPeer.disconnect(Uint32TT reason) : void` | function |
| `enet_peer_disconnect_now` (overload 2) | `NetworkPeer.disconnectNow(...)` | `NetworkPeer.disconnectNow() : void` | function |
| `enet_peer_get_stats` | `NetworkPeer.getStats(...)` | `NetworkPeer.getStats(float[] rtt, float[] packetLoss, Uint32TT sent, Uint32TT received) : void` | function |
| `enet_host_flush` (overload 2) | `NetworkHost.flush(...)` | `NetworkHost.flush() : void` | function |
| `enet_host_broadcast` (overload 2) | `NetworkHost.broadcast(...)` | `NetworkHost.broadcast(Uint8 channelId, List<Uint8> data) : void` | function |
| `enet_host_set_protocol_id` | `NetworkHost.setProtocolId(...)` | `NetworkHost.setProtocolId(Uint16TT protocolId) : void` | function |
| `enet_host_get_protocol_id` | `NetworkHost.getProtocolId(...)` | `NetworkHost.getProtocolId() : Uint16TT` | function |
| `enet_host_create_with_transport` (overload 2) | `NetworkAddress.hostCreateWithTransport(...)` | `NetworkAddress.hostCreateWithTransport(long peerCount, long channelLimit, int incomingBandwidth, int outgoingBandwidth, NetworkTransportType transport) : NetworkHost` | function |
| `enet_host_connect` (overload 3) | `NetworkHost.connect(...)` | `NetworkHost.connect(NetworkAddress value, S izeT, U int32T) : NetworkPeer` | function |
| `enet_host_connect_ex` (overload 2) | `NetworkHost.connectEx(...)` | `NetworkHost.connectEx(String endpoint, S izeT, U int32T) : NetworkPeer` | function |
| `enet_host_transport_caps` (overload 2) | `NetworkHost.transportCaps(...)` | `NetworkHost.transportCaps() : int` | function |
| `enet_http_server_start` | `Network.httpServerStart(...)` | `Network.httpServerStart(String bindAddress, NetworkUint16T port) : NetworkHttpServer` | function |
| `enet_http_server_stop` | `NetworkHttpServer.stop(...)` | `NetworkHttpServer.stop() : void` | function |
| `enet_http_register_endpoint` | `NetworkHttpServer.httpRegisterEndpoint(...)` | `NetworkHttpServer.httpRegisterEndpoint(String path, NetworkHttpEndpointHandler handler) : void` | function |
| `enet_host_create_secure` | `NetworkAddress.hostCreateSecure(...)` | `NetworkAddress.hostCreateSecure(long peerCount, long channelLimit, int incomingBandwidth, int outgoingBandwidth, NetworkTransportType transport, NetworkTransportSecurity security) : NetworkHost` | function |
| `enet_http_request_async` | `Network.httpRequestAsync(...)` | `Network.httpRequestAsync(NetworkHttpRequestDesc desc, NetworkHttpResponseCallback cb) : NetworkHttpRequest` | function |
| `enet_http_request_cancel` | `NetworkHttpRequest.cancel(...)` | `NetworkHttpRequest.cancel() : void` | function |
| `enet_http_request_release` | `NetworkHttpRequest.release(...)` | `NetworkHttpRequest.release() : void` | function |
| `enet_host_set_connect_validator` | `NetworkHost.setConnectValidator(...)` | `NetworkHost.setConnectValidator(NetworkConnectValidator validator) : void` | function |
| `enet_transport_send_stream` | `Network.transportSendStream(...)` | `Network.transportSendStream(... args) : int` | function |
| `enet_transport_receive_stream` | `Network.transportReceiveStream(...)` | `Network.transportReceiveStream(... args) : int` | function |
| `enet_transport_ws_close` | `NetworkTransportPeer.transportWsClose(...)` | `NetworkTransportPeer.transportWsClose(int code) : void` | function |
| `enet_transport_webrtc_signal` | `NetworkWebRTCSignal.transportWebRtc(...)` | `NetworkWebRTCSignal.transportWebRtc(NetworkTransport value, NetworkTransportPeer value) : void` | function |
| `enet_host_set_webrtc_signal_callback` | `NetworkHost.setWebRtcSignalCallback(...)` | `NetworkHost.setWebRtcSignalCallback(E netWebRTCSignalCallback, ByteBuffer value) : void` | function |
| `enet_webrtc_handle_signal` | `NetworkWebRTCSignal.webRtcHandle(...)` | `NetworkWebRTCSignal.webRtcHandle(NetworkHost value, NetworkPeer value) : int` | function |

### `Moss_Physics.h` → `moss.physics`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `CollideShapeResult_FreeMembers` | `CollideShapeResult.freeMembers(...)` | `CollideShapeResult.freeMembers() : void` | function |
| `CollisionEstimationResult_FreeMembers` | `CollisionEstimationResult.freeMembers(...)` | `CollisionEstimationResult.freeMembers() : void` | function |
| `BroadPhaseLayerInterfaceMask_Create` | `Physics.createBroadPhaseLayerInterfaceMask(...)` | `Physics.createBroadPhaseLayerInterfaceMask(int numBroadPhaseLayers) : BroadPhaseLayerInterface` | function |
| `BroadPhaseLayerInterfaceMask_ConfigureLayer` | `BroadPhaseLayerInterface.maskConfigureLayer(...)` | `BroadPhaseLayerInterface.maskConfigureLayer(BroadPhaseLayer broadPhaseLayer, int groupsToInclude, int groupsToExclude) : void` | function |
| `BroadPhaseLayerInterfaceTable_Create` | `Physics.createBroadPhaseLayerInterfaceTable(...)` | `Physics.createBroadPhaseLayerInterfaceTable(int numObjectLayers, int numBroadPhaseLayers) : BroadPhaseLayerInterface` | function |
| `BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer` | `BroadPhaseLayerInterface.tableMapObjectToBroadPhaseLayer(...)` | `BroadPhaseLayerInterface.tableMapObjectToBroadPhaseLayer(ObjectLayer objectLayer, BroadPhaseLayer broadPhaseLayer) : void` | function |
| `ObjectLayerPairFilterMask_Create` | `Physics.createObjectLayerPairFilterMask(...)` | `Physics.createObjectLayerPairFilterMask() : ObjectLayerPairFilter` | function |
| `ObjectLayerPairFilterMask_GetObjectLayer` | `Physics.objectLayerPairFilterMaskGetObjectLayer(...)` | `Physics.objectLayerPairFilterMaskGetObjectLayer(int group, int mask) : ObjectLayer` | function |
| `ObjectLayerPairFilterMask_GetGroup` | `Physics.objectLayerPairFilterMaskGetGroup(...)` | `Physics.objectLayerPairFilterMaskGetGroup(ObjectLayer layer) : int` | function |
| `ObjectLayerPairFilterMask_GetMask` | `Physics.objectLayerPairFilterMaskGetMask(...)` | `Physics.objectLayerPairFilterMaskGetMask(ObjectLayer layer) : int` | function |
| `ObjectLayerPairFilterTable_Create` | `Physics.createObjectLayerPairFilterTable(...)` | `Physics.createObjectLayerPairFilterTable(int numObjectLayers) : ObjectLayerPairFilter` | function |
| `ObjectLayerPairFilterTable_DisableCollision` | `ObjectLayerPairFilter.tableDisableCollision(...)` | `ObjectLayerPairFilter.tableDisableCollision(ObjectLayer layer1, ObjectLayer layer2) : void` | function |
| `ObjectLayerPairFilterTable_EnableCollision` | `ObjectLayerPairFilter.tableEnableCollision(...)` | `ObjectLayerPairFilter.tableEnableCollision(ObjectLayer layer1, ObjectLayer layer2) : void` | function |
| `ObjectLayerPairFilterTable_ShouldCollide` | `ObjectLayerPairFilter.tableShouldCollide(...)` | `ObjectLayerPairFilter.tableShouldCollide(ObjectLayer layer1, ObjectLayer layer2) : boolean` | function |
| `ObjectVsBroadPhaseLayerFilterMask_Create` | `BroadPhaseLayerInterface.createObjectVsBroadPhaseLayerFilterMask(...)` | `BroadPhaseLayerInterface.createObjectVsBroadPhaseLayerFilterMask() : ObjectVsBroadPhaseLayerFilter` | function |
| `ObjectVsBroadPhaseLayerFilterTable_Create` | `BroadPhaseLayerInterface.createObjectVsBroadPhaseLayerFilterTable(...)` | `BroadPhaseLayerInterface.createObjectVsBroadPhaseLayerFilterTable(int numBroadPhaseLayers, ObjectLayerPairFilter objectLayerPairFilter, int numObjectLayers) : ObjectVsBroadPhaseLayerFilter` | function |
| `DrawSettings_InitDefault` | `DrawSettings.initDefault(...)` | `DrawSettings.initDefault() : void` | function |
| `ContactManifold_GetWorldSpaceNormal` | `ContactManifold.getWorldSpaceNormal(...)` | `ContactManifold.getWorldSpaceNormal(Vec3 result) : void` | function |
| `ContactManifold_GetPenetrationDepth` | `ContactManifold.getPenetrationDepth(...)` | `ContactManifold.getPenetrationDepth() : float` | function |
| `ContactManifold_GetSubShapeID1` | `ContactManifold.getSubShapeId1(...)` | `ContactManifold.getSubShapeId1() : SubShapeId` | function |
| `ContactManifold_GetSubShapeID2` | `ContactManifold.getSubShapeId2(...)` | `ContactManifold.getSubShapeId2() : SubShapeId` | function |
| `ContactManifold_GetPointCount` | `ContactManifold.getPointCount(...)` | `ContactManifold.getPointCount() : int` | function |
| `ContactManifold_GetWorldSpaceContactPointOn1` | `ContactManifold.getWorldSpaceContactPointOn1(...)` | `ContactManifold.getWorldSpaceContactPointOn1(int index, Vec3 result) : void` | function |
| `ContactManifold_GetWorldSpaceContactPointOn2` | `ContactManifold.getWorldSpaceContactPointOn2(...)` | `ContactManifold.getWorldSpaceContactPointOn2(int index, Vec3 result) : void` | function |
| `CollisionDispatch_CollideShapeVsShape` | `CollideShapeCollectorCallback.collisionDispatchCollideShapeVsShape(...)` | `CollideShapeCollectorCallback.collisionDispatchCollideShapeVsShape(Shape shape1, Shape shape2, Vec3 scale1, Vec3 scale2, Mat44 centerOfMassTransform1, Mat44 centerOfMassTransform2, CollideShapeSettings collideShapeSettings, ShapeFilter shapeFilter) : boolean` | function |
| `CollisionDispatch_CastShapeVsShapeLocalSpace` | `CastShapeCollectorCallback.collisionDispatchCastShapeVsShapeLocalSpace(...)` | `CastShapeCollectorCallback.collisionDispatchCastShapeVsShapeLocalSpace(Vec3 direction, Shape shape1, Shape shape2, Vec3 scale1InShape2LocalSpace, Vec3 scale2, Mat44 centerOfMassTransform1InShape2LocalSpace, Mat44 centerOfMassWorldTransform2, ShapeCastSettings shapeCastSettings, ShapeFilter shapeFilter) : boolean` | function |
| `CollisionDispatch_CastShapeVsShapeWorldSpace` | `CastShapeCollectorCallback.collisionDispatchCastShapeVsShapeWorldSpace(...)` | `CastShapeCollectorCallback.collisionDispatchCastShapeVsShapeWorldSpace(Vec3 direction, Shape shape1, Shape shape2, Vec3 scale1, Vec3 inScale2, Mat44 centerOfMassWorldTransform1, Mat44 centerOfMassWorldTransform2, ShapeCastSettings shapeCastSettings, ShapeFilter shapeFilter) : boolean` | function |
| `Skeleton_Create` | `Physics.createSkeleton(...)` | `Physics.createSkeleton() : Skeleton` | function |
| `Skeleton_Destroy` | `Skeleton.close(...)` | `Skeleton.close() : void` | function |
| `Skeleton_AddJoint` | `Skeleton.addJoint(...)` | `Skeleton.addJoint(String name) : int` | function |
| `Skeleton_AddJoint2` | `Skeleton.addJoint2(...)` | `Skeleton.addJoint2(String name, int parentIndex) : int` | function |
| `Skeleton_AddJoint3` | `Skeleton.addJoint3(...)` | `Skeleton.addJoint3(String name, String parentName) : int` | function |
| `Skeleton_GetJointCount` | `Skeleton.getJointCount(...)` | `Skeleton.getJointCount() : int` | function |
| `Skeleton_GetJoint` | `SkeletonJoint.skeletonGet(...)` | `SkeletonJoint.skeletonGet(Skeleton skeleton, int index) : void` | function |
| `Skeleton_GetJointIndex` | `Skeleton.getJointIndex(...)` | `Skeleton.getJointIndex(String name) : int` | function |
| `Skeleton_CalculateParentJointIndices` | `Skeleton.calculateParentJointIndices(...)` | `Skeleton.calculateParentJointIndices() : void` | function |
| `Skeleton_AreJointsCorrectlyOrdered` | `Skeleton.areJointsCorrectlyOrdered(...)` | `Skeleton.areJointsCorrectlyOrdered() : boolean` | function |
| `Indexify` | `Physics.indexify(...)` | `Physics.indexify(TriangleList inTriangles, VertexList outVertices, IndexedTriangleList outTriangles, float inVertexWeldDistance) : void` | function |
| `Deindexify` | `Physics.deindexify(...)` | `Physics.deindexify(VertexList inVertices, IndexedTriangleList inTriangles, TriangleList outTriangles) : void` | function |

### `Moss_Platform.h` → `moss.platform`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_CreateWindow` | `Platform.createWindow(...)` | `public static Window Platform.createWindow(WindowOptions options)` | function |
| `Moss_TerminateWindow` | `Window.close(...)` | `Window.close() : void` | function |
| `Moss_CreateMessageBox` | `Window.showMessageBox(...)` | `Window.showMessageBox(String title, String message, MessageBoxFlags flags) : boolean` | function |
| `Moss_ShouldWindowClose` | `Window.shouldClose()` | `public boolean Window.shouldClose()` | function |
| `Moss_PollEvents` | `Platform.pollEvents(...)` | `Platform.pollEvents() : void` | function |
| `Moss_GetWindowWidth` | `Platform.getWindowWidth(...)` | `Platform.getWindowWidth() : int` | function |
| `Moss_GetWindowHeight` | `Platform.getWindowHeight(...)` | `Platform.getWindowHeight() : int` | function |
| `Moss_SetWindowTitle` | `Window.getTitle() / Window.setTitle(...)` | `public String Window.getTitle(); public void Window.setTitle(String title)` | function |
| `Moss_SetWindowIcon` | `Window.setIcon(...)` | `Window.setIcon(Image image) : void` | function |
| `Moss_CloseWindow` | `Window.close(...)` | `Window.close() : void` | function |
| `Moss_MonitorGetPrimary` | `Platform.Monitors.monitorGetPrimary(...)` | `Platform.Monitors.monitorGetPrimary() : Monitor` | function |
| `Moss_MonitorGetSecondary` | `Platform.Monitors.monitorGetSecondary(...)` | `Platform.Monitors.monitorGetSecondary() : Monitor` | function |
| `Moss_MonitorGetPhysicalSize` | `Monitor.getPhysicalSize(...)` | `Monitor.getPhysicalSize(int[] widthMm, int[] heightMm) : void` | function |
| `Moss_MonitorGetContentScale` | `Monitor.getContentScale(...)` | `Monitor.getContentScale(float[] xscale, float[] yscale) : void` | function |
| `Moss_MonitorGetPosition` | `Monitor.getPosition(...)` | `Monitor.getPosition(int[] x, int[] y) : void` | function |
| `Moss_MonitorGetName` | `Monitor.getName(...)` | `Monitor.getName() : String` | function |
| `Moss_MonitorSetGammaRamp` | `GammaRamp.monitorSet(...)` | `GammaRamp.monitorSet(Monitor monitor) : void` | function |
| `Moss_MonitorGetGammaRamp` | `Monitor.getGammaRamp(...)` | `Monitor.getGammaRamp() : GammaRamp` | function |
| `Moss_MonitorSetGamma` | `Monitor.setGamma(...)` | `Monitor.setGamma(float gamma) : void` | function |
| `Moss_IsKeyPressed` | `Platform.Input.isKeyPressed(...)` | `Platform.Input.isKeyPressed(Keyboard key) : boolean` | function |
| `Moss_IsReleased` | `Platform.isReleased(...)` | `Platform.isReleased(Keyboard key) : boolean` | function |
| `Moss_IsKeyJustPressed` | `Platform.Input.isKeyJustPressed(...)` | `Platform.Input.isKeyJustPressed(Keyboard key) : boolean` | function |
| `Moss_IsKeyJustReleased` | `Platform.Input.isKeyJustReleased(...)` | `Platform.Input.isKeyJustReleased(Keyboard key) : boolean` | function |
| `Moss_InputGetKey` | `Platform.Input.inputGetKey(...)` | `Platform.Input.inputGetKey() : Keyboard` | function |
| `Moss_IsMousePressed` | `Platform.Input.isMousePressed(...)` | `Platform.Input.isMousePressed(Mouse button) : boolean` | function |
| `Moss_IsMouseReleased` | `Platform.Input.isMouseReleased(...)` | `Platform.Input.isMouseReleased(Mouse button) : boolean` | function |
| `Moss_IsMouseJustPressed` | `Platform.Input.isMouseJustPressed(...)` | `Platform.Input.isMouseJustPressed(Mouse button) : boolean` | function |
| `Moss_IsMouseJustReleased` | `Platform.Input.isMouseJustReleased(...)` | `Platform.Input.isMouseJustReleased(Mouse button) : boolean` | function |
| `Moss_InputGetMouseButton` | `Platform.Input.inputGetMouseButton(...)` | `Platform.Input.inputGetMouseButton() : Mouse` | function |
| `Moss_GetMousePosition` | `Platform.Input.getMousePosition(...)` | `public static Point Platform.Input.getMousePosition()` | function |
| `Moss_SetMousePosition` | `Platform.Input.setMousePosition(...)` | `Platform.Input.setMousePosition(int x, int y) : void` | function |
| `Moss_SetMouseVisible` | `Platform.Input.setMouseVisible(...)` | `Platform.Input.setMouseVisible(boolean visible) : void` | function |
| `Moss_GetNumGamepads` | `Platform.getNumGamepads(...)` | `Platform.getNumGamepads() : int` | function |
| `Moss_OpenGamepad` | `Platform.Gamepads.openGamepad(...)` | `Platform.Gamepads.openGamepad(GamepadId id) : Gamepad` | function |
| `Moss_CloseGamepad` | `Gamepad.close(...)` | `Gamepad.close() : void` | function |
| `Moss_GamepadConnected` | `Gamepad.connected(...)` | `Gamepad.connected() : boolean` | function |
| `Moss_UpdateGamepads` | `Platform.updateGamepads(...)` | `Platform.updateGamepads() : void` | function |
| `Moss_IsGamepadButtonPressed` | `Gamepad.isButtonPressed(...)` | `Gamepad.isButtonPressed(GamepadButton button) : boolean` | function |
| `Moss_IsGamepadButtonJustPressed` | `Gamepad.isButtonJustPressed(...)` | `Gamepad.isButtonJustPressed(GamepadButton button) : boolean` | function |
| `Moss_IsGamepadButtonJustReleased` | `Gamepad.isButtonJustReleased(...)` | `Gamepad.isButtonJustReleased(GamepadButton button) : boolean` | function |
| `Moss_GetGamepadAxis` | `Gamepad.getAxis(...)` | `Gamepad.getAxis(GamepadAxis axis) : float` | function |
| `Moss_GetGamepadOrientation` | `Gamepad.getOrientation(...)` | `Gamepad.getOrientation(float[] w, float[] x, float[] y, float[] z) : boolean` | function |
| `Moss_GetGamepadPlayerSpaceGyro` | `Gamepad.getPlayerSpaceGyro(...)` | `Gamepad.getPlayerSpaceGyro(float[] outX, float[] outY, float yawRelaxFactor) : void` | function |
| `Moss_GetGamepadWorldSpaceGyro` | `Gamepad.getWorldSpaceGyro(...)` | `Gamepad.getWorldSpaceGyro(float[] outX, float[] outY, float sideReductionThreshold) : void` | function |
| `Moss_ResetGamepadCalibration` | `Gamepad.resetCalibration(...)` | `Gamepad.resetCalibration() : void` | function |
| `Moss_RumbleGamepad` | `Gamepad.rumble(...)` | `Gamepad.rumble(int low, int high, int durationMs) : boolean` | function |
| `Moss_RumbleGamepadTriggers` | `Gamepad.rumbleTriggers(...)` | `Gamepad.rumbleTriggers(int left, int right, int durationMs) : boolean` | function |
| `Moss_SetGamepadLED` | `Gamepad.setLed(...)` | `Gamepad.setLed(byte r, byte g, byte b) : boolean` | function |
| `Moss_GetGamepadName` | `Gamepad.getName(...)` | `Gamepad.getName() : String` | function |
| `Moss_GetGamepadID` | `Gamepad.getId(...)` | `Gamepad.getId() : GamepadId` | function |
| `Moss_GetGamepadPlayerIndex` | `Gamepad.getPlayerIndex(...)` | `Gamepad.getPlayerIndex() : int` | function |
| `Moss_GetGamepadPowerInfo` | `Gamepad.getPowerInfo(...)` | `Gamepad.getPowerInfo(int[] percent) : PowerState` | function |
| `Moss_GetNumGamepadTouchpads` | `Gamepad.getNumTouchpads(...)` | `Gamepad.getNumTouchpads() : int` | function |
| `Moss_GetNumGamepadTouchpadFingers` | `Gamepad.getNumTouchpadFingers(...)` | `Gamepad.getNumTouchpadFingers() : int` | function |
| `Moss_GetGamepadTouchpadFinger` | `Gamepad.getTouchpadFinger(...)` | `Gamepad.getTouchpadFinger(int pad, int finger, boolean[] down, float[] x, float[] y, float[] pressure) : boolean` | function |
| `Moss_GetGamepadMapping` | `Gamepad.getMapping(...)` | `Gamepad.getMapping() : String` | function |
| `Moss_SetGamepadMapping` | `Gamepad.setMapping(...)` | `Gamepad.setMapping(String mapping) : boolean` | function |
| `Moss_ReloadGamepadMappings` | `Platform.Gamepads.reloadGamepadMappings(...)` | `Platform.Gamepads.reloadGamepadMappings() : void` | function |
| `Moss_InputGetGamepadButton` | `Platform.Gamepads.inputGetGamepadButton(...)` | `Platform.Gamepads.inputGetGamepadButton() : GamepadButton` | function |
| `Moss_InputGetGamepadAxis` | `Platform.Gamepads.inputGetGamepadAxis(...)` | `Platform.Gamepads.inputGetGamepadAxis() : GamepadAxis` | function |
| `Moss_GetPenDeviceType` | `Platform.Input.getPenDeviceType(...)` | `Platform.Input.getPenDeviceType(PenId instanceId) : PenDeviceType` | function |
| `Moss_GetTouchDeviceName` | `Platform.Input.getTouchDeviceName(...)` | `Platform.Input.getTouchDeviceName(TouchId touchId) : String` | function |
| `Moss_GetTouchDevices` | `Platform.Input.getTouchDevices(...)` | `Platform.Input.getTouchDevices(int[] count) : TouchId` | function |
| `Moss_GetTouchDeviceType` | `Platform.Input.getTouchDeviceType(...)` | `Platform.Input.getTouchDeviceType(TouchId touchId) : TouchDeviceType` | function |
| `Moss_GetTouchFingers` | `Platform.Input.getTouchFingers(...)` | `Platform.Input.getTouchFingers(TouchId touchId, int[] count) : Finger` | function |
| `Moss_GamepadHasSensor` | `Gamepad.hasSensor(...)` | `Gamepad.hasSensor(GamepadSensorType sensor) : boolean` | function |
| `Moss_SetGamepadSensorEnabled` | `Gamepad.setSensorEnabled(...)` | `Gamepad.setSensorEnabled(GamepadSensorType sensor, boolean enabled) : boolean` | function |
| `Moss_GetGamepadSensorData` | `GamepadSensorData.get(...)` | `GamepadSensorData.get(Gamepad gp, GamepadSensorType sensor) : boolean` | function |
| `Moss_OpenHaptic` | `Platform.Haptics.openHaptic(...)` | `Platform.Haptics.openHaptic(HapticId id) : Haptic` | function |
| `Moss_CloseHaptic` | `Haptic.close(...)` | `Haptic.close() : void` | function |
| `Moss_CreateHapticEffect` | `Haptic.createEffect(...)` | `Haptic.createEffect() : HapticId` | function |
| `Moss_DestroyHapticEffect` | `Haptic.destroyEffect(...)` | `Haptic.destroyEffect() : void` | function |
| `Moss_GetHapticEffectStatus` | `Haptic.getEffectStatus(...)` | `Haptic.getEffectStatus() : boolean` | function |
| `Moss_GetHapticFeatures` | `Haptic.getFeatures(...)` | `Haptic.getFeatures() : int` | function |
| `Moss_GetHapticFromID` | `Haptic.getFromId(...)` | `Haptic.getFromId() : Haptic` | function |
| `Moss_GetHapticID` | `Haptic.getId(...)` | `Haptic.getId() : HapticId` | function |
| `Moss_GetHapticName` | `Haptic.getName(...)` | `Haptic.getName() : String` | function |
| `Moss_GetHapticNameForID` | `Haptic.getNameForId(...)` | `Haptic.getNameForId() : String` | function |
| `Moss_GetHaptics` | `Haptic.getHaptics(...)` | `Haptic.getHaptics() : HapticId` | function |
| `Moss_GetMaxHapticEffects` | `Haptic.getMaxEffects(...)` | `Haptic.getMaxEffects() : int` | function |
| `Moss_GetMaxHapticEffectsPlaying` | `Haptic.getMaxEffectsPlaying(...)` | `Haptic.getMaxEffectsPlaying() : int` | function |
| `Moss_GetNumHapticAxes` | `Haptic.getNumAxes(...)` | `Haptic.getNumAxes() : int` | function |
| `Moss_HapticEffectSupported` | `Haptic.effectSupported(...)` | `Haptic.effectSupported() : boolean` | function |
| `Moss_HapticRumbleSupported` | `Haptic.rumbleSupported(...)` | `Haptic.rumbleSupported() : boolean` | function |
| `Moss_InitHapticRumble` | `Haptic.initRumble(...)` | `Haptic.initRumble() : boolean` | function |
| `Moss_IsJoystickHaptic` | `Gamepad.isJoystickHaptic(...)` | `Gamepad.isJoystickHaptic() : boolean` | function |
| `Moss_IsMouseHaptic` | `Platform.Haptics.isMouseHaptic(...)` | `Platform.Haptics.isMouseHaptic() : boolean` | function |
| `Moss_OpenHapticFromJoystick` | `Gamepad.openHapticFromJoystick(...)` | `Gamepad.openHapticFromJoystick() : Haptic` | function |
| `Moss_OpenHapticFromMouse` | `Platform.Haptics.openHapticFromMouse(...)` | `Platform.Haptics.openHapticFromMouse() : Haptic` | function |
| `Moss_PauseHaptic` | `Haptic.pause(...)` | `Haptic.pause() : boolean` | function |
| `Moss_PlayHapticRumble` | `Haptic.playRumble(...)` | `Haptic.playRumble(float strength, int length) : boolean` | function |
| `Moss_ResumeHaptic` | `Haptic.resume(...)` | `Haptic.resume() : boolean` | function |
| `Moss_RunHapticEffect` | `Haptic.runEffect(...)` | `Haptic.runEffect(int iterations) : boolean` | function |
| `Moss_SetHapticAutocenter` | `Haptic.setAutocenter(...)` | `Haptic.setAutocenter(int center) : boolean` | function |
| `Moss_SetHapticGain` | `Haptic.setGain(...)` | `Haptic.setGain(int gain) : boolean` | function |
| `Moss_StopHapticEffect` | `Haptic.stopEffect(...)` | `Haptic.stopEffect(HapticEffectId effect) : boolean` | function |
| `Moss_StopHapticEffects` | `Haptic.stopEffects(...)` | `Haptic.stopEffects() : boolean` | function |
| `Moss_StopHapticRumble` | `Haptic.stopRumble(...)` | `Haptic.stopRumble() : boolean` | function |
| `Moss_UpdateHapticEffect` | `HapticEffect.update(...)` | `HapticEffect.update(Haptic haptic, HapticEffectId effect) : boolean` | function |
| `Moss_GetAvailableCPUCores` | `Platform.getAvailableCpuCores(...)` | `Platform.getAvailableCpuCores() : int` | function |
| `Moss_GetCPUCacheLineSize` | `Platform.getCpuCacheLineSize(...)` | `Platform.getCpuCacheLineSize() : int` | function |
| `Moss_GetSystemRAM` | `Platform.getSystemRam(...)` | `Platform.getSystemRam() : int` | function |
| `Moss_OpenURL` | `Platform.openUrl(...)` | `Platform.openUrl(String url) : boolean` | function |
| `Moss_GetLocale` | `Platform.getLocale(...)` | `Platform.getLocale() : Locale` | function |
| `Moss_GetPowerInfo` | `Platform.getPowerInfo(...)` | `Platform.getPowerInfo(int[] seconds, int[] percent) : PowerState` | function |
| `Moss_IsProcessRunningByName` | `Platform.isProcessRunningByName(...)` | `Platform.isProcessRunningByName(String executablePath) : boolean` | function |
| `Moss_LoadDynamicLibrary` | `Platform.loadDynamicLibrary(...)` | `Platform.loadDynamicLibrary(String libPath) : ByteBuffer` | function |
| `Moss_GetLibrarySymbol` | `Platform.getLibrarySymbol(...)` | `Platform.getLibrarySymbol(ByteBuffer handle, String symbolName) : ByteBuffer` | function |
| `Moss_UnloadDynamicLibrary` | `Platform.unloadDynamicLibrary(...)` | `Platform.unloadDynamicLibrary(ByteBuffer handle) : void` | function |
| `Moss_SetFramebufferResizeCallback` | `Platform.setFramebufferResizeCallback(...)` | `Platform.setFramebufferResizeCallback(FramebufferResizeCallback callback) : void` | function |
| `Moss_SetWindowSizeCallback` | `Platform.setWindowSizeCallback(...)` | `Platform.setWindowSizeCallback(WindowSizeCallback callback) : void` | function |
| `Moss_SetWindowResizeCallback` | `Platform.setWindowResizeCallback(...)` | `Platform.setWindowResizeCallback(WindowResizeCallback callback) : void` | function |
| `Moss_SetWindowPositionCallback` | `Platform.setWindowPositionCallback(...)` | `Platform.setWindowPositionCallback(WindowPositionCallback callback) : void` | function |
| `Moss_SetWindowFocusCallback` | `Platform.setWindowFocusCallback(...)` | `Platform.setWindowFocusCallback(WindowFocusCallback callback) : void` | function |
| `Moss_SetWindowContentScaleCallback` | `Platform.setWindowContentScaleCallback(...)` | `Platform.setWindowContentScaleCallback(WindowContentScaleCallback callback) : void` | function |
| `Moss_SetMonitorCallback` | `Platform.Monitors.setMonitorCallback(...)` | `Platform.Monitors.setMonitorCallback(MonitorCallback callback) : void` | function |
| `Moss_GetCameras` | `Platform.getCameras(...)` | `Platform.getCameras(int[] count) : CameraId` | function |
| `Moss_GetCameraName` | `Platform.Cameras.getCameraName(...)` | `Platform.Cameras.getCameraName(CameraId cameraId) : String` | function |
| `Moss_GetCameraPosition` | `Platform.Cameras.getCameraPosition(...)` | `Platform.Cameras.getCameraPosition(CameraId cameraId) : CameraPosition` | function |
| `Moss_GetCurrentCameraDriver` | `Platform.Cameras.getCurrentCameraDriver(...)` | `Platform.Cameras.getCurrentCameraDriver() : String` | function |
| `Moss_GetNumCameraDrivers` | `Platform.Cameras.getNumCameraDrivers(...)` | `Platform.Cameras.getNumCameraDrivers() : int` | function |
| `Moss_GetCameraSupportedFormats` | `Platform.Cameras.getCameraSupportedFormats(...)` | `Platform.Cameras.getCameraSupportedFormats(CameraId cameraId, int[] count) : CameraSpec` | function |
| `Moss_AcquireCameraFrame` | `Capture.acquireCameraFrame(...)` | `Capture.acquireCameraFrame(long[] timestampNs) : Surface` | function |
| `Moss_ReleaseCameraFrame` | `Capture.releaseCameraFrame(...)` | `Capture.releaseCameraFrame(Surface frame) : void` | function |
| `Moss_GetCameraFormat` | `Capture.getCameraFormat(...)` | `Capture.getCameraFormat(CameraSpec outSpec) : boolean` | function |
| `Moss_GetCameraPermissionState` | `Capture.getCameraPermissionState(...)` | `Capture.getCameraPermissionState() : CameraPermissionState` | function |
| `Moss_GetCameraProperties` (overload 1) | `Capture.getCameraProperties(...)` | `Capture.getCameraProperties() : PropertiesId` | function |
| `Moss_CloseCamera` | `Capture.closeCamera(...)` | `Capture.closeCamera() : void` | function |
| `Moss_GetCameraID` | `Capture.getCameraId(...)` | `Capture.getCameraId() : CameraId` | function |
| `Moss_GetCameraProperties` (overload 2) | `Capture.getCameraProperties(...)` | `Capture.getCameraProperties() : PropertiesId` | function |
| `Moss_OpenCapture` | `CameraSpec.openCapture(...)` | `CameraSpec.openCapture(CameraId id) : Capture` | function |
| `Moss_CopyFile` | `Platform.Storage.copyFile(...)` | `Platform.Storage.copyFile(String srcPath, String dstPath, boolean overwrite) : boolean` | function |
| `Moss_CreateDirectory` | `Platform.Storage.createDirectory(...)` | `Platform.Storage.createDirectory(String path, boolean recursive) : boolean` | function |
| `Moss_RemovePath` | `Platform.Storage.removePath(...)` | `Platform.Storage.removePath(String path, boolean recursive) : boolean` | function |
| `Moss_RenamePath` | `Platform.Storage.renamePath(...)` | `Platform.Storage.renamePath(String oldPath, String newPath, boolean overwrite) : boolean` | function |
| `Moss_GetPathInfo` | `Platform.Storage.getPathInfo(...)` | `Platform.Storage.getPathInfo(String path, PathInfo outInfo) : boolean` | function |
| `Moss_GetCurrentDirectory` | `Platform.Storage.getCurrentDirectory(...)` | `Platform.Storage.getCurrentDirectory(ByteBuffer outPath, int maxLen) : boolean` | function |
| `Moss_GetBasePath` | `Platform.Storage.getBasePath(...)` | `Platform.Storage.getBasePath(ByteBuffer outPath, int maxLen) : boolean` | function |
| `Moss_GetUserFolder` | `Platform.getUserFolder(...)` | `Platform.getUserFolder(UserFolder folder, ByteBuffer outPath, int maxLen) : boolean` | function |
| `Moss_GetPrefPath` | `Platform.Storage.getPrefPath(...)` | `Platform.Storage.getPrefPath(String orgName, String appName, ByteBuffer outPath, int maxLen) : boolean` | function |
| `Moss_EnumerateDirectory` | `Platform.Storage.enumerateDirectory(...)` | `Platform.Storage.enumerateDirectory(String path, boolean recursive, DirectoryIterateFn callback) : boolean` | function |
| `Moss_GlobDirectory` | `Platform.Storage.globDirectory(...)` | `Platform.Storage.globDirectory(String pattern, DirectoryIterateFn callback) : boolean` | function |
| `Moss_ShowFileDialogWithProperties` | `DialogFileFilter.showFileDialogWithProperties(...)` | `DialogFileFilter.showFileDialogWithProperties(DialogFileCallback callback, Window window, int nfilters, String defaultLocation) : void` | function |
| `Moss_ShowOpenFolderDialog` | `DialogFileFilter.showOpenFolderDialog(...)` | `DialogFileFilter.showOpenFolderDialog(DialogFileCallback callback, Window window, int nfilters, String defaultLocation, boolean allowMany) : void` | function |
| `Moss_ShowOpenFileDialog` | `Window.showOpenFileDialog(...)` | `Window.showOpenFileDialog(DialogFileCallback callback, String defaultLocation, boolean allowMany) : void` | function |
| `Moss_ShowSaveFileDialog` | `Platform.Dialogs.showSaveFileDialog(...)` | `Platform.Dialogs.showSaveFileDialog(FileDialogType type, DialogFileCallback callback, PropertiesId props) : void` | function |
| `Moss_CloseStorage` | `Storage.close(...)` | `Storage.close() : boolean` | function |
| `Moss_CopyStorageFile` | `Storage.copyFile(...)` | `Storage.copyFile(String oldpath, String newpath) : boolean` | function |
| `Moss_CreateStorageDirectory` | `Storage.createDirectory(...)` | `Storage.createDirectory(String path) : boolean` | function |
| `Moss_EnumerateStorageDirectory` | `Storage.enumerateDirectory(...)` | `Storage.enumerateDirectory(String path, EnumerateDirectoryCallback callback) : boolean` | function |
| `Moss_GetStorageFileSize` | `Storage.getFileSize(...)` | `Storage.getFileSize(String path, Uint64 length) : boolean` | function |
| `Moss_GetStorageSpaceRemaining` | `Storage.getSpaceRemaining(...)` | `Storage.getSpaceRemaining() : long` | function |
| `Moss_GetStoragePathInfo` | `Storage.getPathInfo(...)` | `Storage.getPathInfo(String path, PathInfo info) : boolean` | function |
| `Moss_GlobStorageDirectory` | `Storage.globDirectory(...)` | `Storage.globDirectory(String path, String pattern, GlobFlags flags, int[] count) : String` | function |
| `Moss_OpenFileStorage` | `Platform.Storage.openFileStorage(...)` | `Platform.Storage.openFileStorage(String path) : Storage` | function |
| `Moss_OpenStorage` | `Storage.open(...)` | `Storage.open() : Storage` | function |
| `Moss_OpenTitleStorage` | `Platform.Storage.openTitleStorage(...)` | `Platform.Storage.openTitleStorage(String override, PropertiesId props) : Storage` | function |
| `Moss_OpenUserStorage` | `Platform.Storage.openUserStorage(...)` | `Platform.Storage.openUserStorage(String org, String app, PropertiesId props) : Storage` | function |
| `Moss_ReadStorageFile` | `Storage.readFile(...)` | `Storage.readFile(String path, ByteBuffer destination, Uint64 length) : boolean` | function |
| `Moss_RemoveStoragePath` | `Storage.removePath(...)` | `Storage.removePath(String path) : boolean` | function |
| `Moss_RenameStoragePath` | `Storage.renamePath(...)` | `Storage.renamePath(String oldpath, String newpath) : boolean` | function |
| `Moss_StorageReady` | `Storage.ready(...)` | `Storage.ready() : boolean` | function |
| `Moss_WriteStorageFile` | `Storage.writeFile(...)` | `Storage.writeFile(String path, ByteBuffer source, Uint64 length) : boolean` | function |
| `Moss_CreateEmbeddedWindow` | `Platform.createEmbeddedWindow(...)` | `Platform.createEmbeddedWindow() : Window` | function |
| `Moss_Terminate_EmbeddedWindow` | `Platform.terminateEmbeddedWindow(...)` | `Platform.terminateEmbeddedWindow() : void` | function |
| `Moss_EmbeddedWindow_OnOrientationChanged` | `Platform.embeddedWindowOnOrientationChanged(...)` | `Platform.embeddedWindowOnOrientationChanged(boolean focused) : void` | function |
| `Moss_EmbeddedWindow_OnResume` | `Platform.embeddedWindowOnResume(...)` | `Platform.embeddedWindowOnResume() : void` | function |
| `Moss_EmbeddedWindow_OnPause` | `Platform.embeddedWindowOnPause(...)` | `Platform.embeddedWindowOnPause() : void` | function |
| `Moss_MakeContextCurrent` | `Window.makeContextCurrent(...)` | `Window.makeContextCurrent() : void` | function |
| `Moss_SwapBuffers` | `Platform.swapBuffers(...)` | `Platform.swapBuffers() : void` | function |
| `Moss_SwapBuffersInterval` | `Platform.swapBuffersInterval(...)` | `Platform.swapBuffersInterval(int interval) : void` | function |
| `Moss_GetProcAddress` | `Platform.getProcAddress(...)` | `Platform.getProcAddress(String procname) : ByteBuffer` | function |
| `Moss_CreateWindowSurface` | `Window.createVulkanSurface(...)` | `Window.createVulkanSurface(VkInstance instance, VkAllocationCallbacks allocator) : VkSurfaceKHR` | function |
| `Moss_VulkanSupported` | `Platform.supported(...)` | `Platform.supported() : int` | function |
| `Moss_GetRequiredInstanceExtensions` | `Platform.getRequiredInstanceExtensions(...)` | `Platform.getRequiredInstanceExtensions(int[] count) : String` | function |
| `Moss_GetInstanceProcAddress` | `Platform.getInstanceProcAddress(...)` | `Platform.getInstanceProcAddress(VkInstance instance, String procname) : ByteBuffer` | function |
| `Moss_GetPhysicalDevicePresentationSupport` | `Window.getPhysicalDevicePresentationSupport(...)` | `Window.getPhysicalDevicePresentationSupport(VkPhysicalDevice device, int queuefamily) : int` | function |
| `Moss_Metal_CreateView` | `Window.createView(...)` | `Window.createView() : MetalView` | function |
| `Moss_Metal_DestroyView` | `Platform.destroyView(...)` | `Platform.destroyView(MetalView view) : void` | function |
| `Moss_Metal_GetLayer` | `Platform.getLayer(...)` | `Platform.getLayer(MetalView view) : ByteBuffer` | function |
| `Moss_Metal_Resize` | `Platform.resize(...)` | `Platform.resize(MetalView handle, int width, int height) : void` | function |

### `Moss_Renderer.h` → `moss.renderer`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_RendererCreateMaterial` | `Renderer.createMaterial(...)` | `Renderer.createMaterial(MaterialDesc desc) : Material` | function |
| `Moss_RendererDestroyMaterial` | `Renderer.destroyMaterial(...)` | `Renderer.destroyMaterial(Material material) : void` | function |
| `Moss_MaterialSetUniform` | `Material.setUniform(...)` | `Material.setUniform(String name, ByteBuffer data, int size) : int` | function |
| `Moss_MaterialSetTexture` | `Material.setTexture(...)` | `Material.setTexture(MaterialTextureType type, Texture texture, GpuSampler sampler) : int` | function |
| `Moss_RendererCreatePipeline` | `Renderer.createPipeline(...)` | `Renderer.createPipeline(PipelineDesc desc) : PipelineState` | function |
| `Moss_PipelineBind` | `Renderer.pipelineBind(...)` | `Renderer.pipelineBind(PipelineState pipeline) : void` | function |
| `Moss_PipelineUnbind` | `Renderer.pipelineUnbind(...)` | `Renderer.pipelineUnbind(PipelineState pipeline) : void` | function |
| `Moss_PipelineSetUniform` | `Renderer.pipelineSetUniform(...)` | `Renderer.pipelineSetUniform(PipelineState pipeline, String name, ByteBuffer data, int size) : void` | function |
| `Moss_PipelineFlush` | `Renderer.pipelineFlush(...)` | `Renderer.pipelineFlush(PipelineState pipeline) : void` | function |
| `Moss_PipelineValidate` | `Renderer.pipelineValidate(...)` | `Renderer.pipelineValidate(PipelineState pipeline) : int` | function |
| `Moss_RendererCreateResourceSet` | `Renderer.createResourceSet(...)` | `Renderer.createResourceSet(ResourceSetLayoutDesc desc) : ResourceSet` | function |
| `Moss_RendererDestroyResourceSet` | `Renderer.destroyResourceSet(...)` | `Renderer.destroyResourceSet(ResourceSet set) : void` | function |
| `Moss_ResourceSetBindUniformBuffer` | `ResourceSet.bindUniformBuffer(...)` | `ResourceSet.bindUniformBuffer(int binding, GpuBuffer buffer) : void` | function |
| `Moss_ResourceSetBindUniformBufferRange` | `ResourceSet.bindUniformBufferRange(...)` | `ResourceSet.bindUniformBufferRange(int binding, GpuBuffer buffer, long offset, long size) : void` | function |
| `Moss_ResourceSetBindStorageBuffer` | `ResourceSet.bindStorageBuffer(...)` | `ResourceSet.bindStorageBuffer(int binding, GpuBuffer buffer) : void` | function |
| `Moss_ResourceSetBindTexture` | `ResourceSet.bindTexture(...)` | `ResourceSet.bindTexture(int binding, Texture texture) : void` | function |
| `Moss_ResourceSetBindSampler` | `ResourceSet.bindSampler(...)` | `ResourceSet.bindSampler(int binding, GpuSampler sampler) : void` | function |
| `Moss_ResourceSetBind` | `Renderer.resourceSetBind(...)` | `Renderer.resourceSetBind(PipelineState pipeline, int setIndex, ResourceSet set) : void` | function |
| `Moss_RendererCreateFramebuffer` | `Renderer.createFramebuffer(...)` | `Renderer.createFramebuffer(FramebufferDesc desc) : Framebuffer` | function |
| `Moss_RendererDestroyFramebuffer` | `Renderer.destroyFramebuffer(...)` | `Renderer.destroyFramebuffer(Framebuffer framebuffer) : void` | function |
| `Moss_FramebufferResize` | `Framebuffer.resize(...)` | `Framebuffer.resize(int width, int height) : void` | function |
| `Moss_PostProcessExecute` | `Renderer.postProcessExecute(...)` | `Renderer.postProcessExecute(PostProcessPass pass) : void` | function |
| `Moss_FramebufferBegin` | `Renderer.framebufferBegin(...)` | `Renderer.framebufferBegin(Framebuffer framebuffer) : void` | function |
| `Moss_FramebufferEnd` | `Renderer.framebufferEnd(...)` | `Renderer.framebufferEnd(Framebuffer framebuffer) : void` | function |
| `Moss_TextureBarrier` | `Renderer.textureBarrier(...)` | `Renderer.textureBarrier(Texture texture, ResourceState oldState, ResourceState newState) : void` | function |
| `Moss_RendererGetBackbuffer` | `Renderer.getBackbuffer(...)` | `Renderer.getBackbuffer() : Framebuffer` | function |
| `Moss_RenderSubViewport` | `SubViewport.render(...)` | `SubViewport.render(GpuCommandBuffer cmd, SubViewportRecordFn record) : void` | function |
| `Moss_RendererCreate` | `Renderer.create(...)` | `Renderer.create(RendererDesc desc) : Renderer` | function |
| `Moss_CreateRenderer` | `Renderer.createRenderer(...)` | `Renderer.createRenderer(Window window) : Renderer` | function |
| `Moss_RendererDestroy` | `Renderer.close(...)` | `Renderer.close() : void` | function |
| `Moss_TerminateRenderer` | `Renderer.close(...)` | `Renderer.close() : void` | function |
| `Moss_RendererBeginFrame` | `Renderer.beginFrame(...)` | `Renderer.beginFrame() : void` | function |
| `Moss_RendererBeginFrameEx` | `Renderer.beginFrameEx(...)` | `Renderer.beginFrameEx(RenderFrameDesc desc) : void` | function |
| `Moss_RendererEndFrame` | `Renderer.endFrame(...)` | `Renderer.endFrame() : void` | function |
| `Moss_RendererGetGPUDevice` | `Renderer.getGpuDevice(...)` | `Renderer.getGpuDevice() : GpuDevice` | function |
| `Moss_MeshLoadFBX` | `Renderer.meshLoadFbx(...)` | `Renderer.meshLoadFbx(String filePath) : Mesh` | function |
| `Moss_MeshLoadOBJ` | `Renderer.meshLoadObj(...)` | `Renderer.meshLoadObj(String filePath) : Mesh` | function |
| `Moss_MeshLoadGLB` | `Renderer.meshLoadGlb(...)` | `Renderer.meshLoadGlb(String filePath) : Mesh` | function |
| `Moss_ModelLoadFBX` | `Renderer.modelLoadFbx(...)` | `Renderer.modelLoadFbx(String filePath) : Model` | function |
| `Moss_ModelLoadOBJ` | `Renderer.modelLoadObj(...)` | `Renderer.modelLoadObj(String filePath) : Model` | function |
| `Moss_ModelLoadGLB` | `Renderer.modelLoadGlb(...)` | `Renderer.modelLoadGlb(String filePath) : Model` | function |
| `Moss_ModelLoad` | `Renderer.modelLoad(...)` | `Renderer.modelLoad(String filePath) : Model` | function |
| `Moss_ModelGetLastLoadError` | `Renderer.modelGetLastLoadError(...)` | `Renderer.modelGetLastLoadError() : ModelLoadError` | function |
| `Moss_ModelGetLastLoadErrorMessage` | `Renderer.modelGetLastLoadErrorMessage(...)` | `Renderer.modelGetLastLoadErrorMessage() : String` | function |
| `Moss_ModelFindBone` | `Model.findBone(...)` | `Model.findBone(String name) : int` | function |
| `Moss_ModelFindMorphTarget` | `Model.findMorphTarget(...)` | `Model.findMorphTarget(String name) : int` | function |
| `Moss_ModelSetBoneTransform` | `Model.setBoneTransform(...)` | `Model.setBoneTransform(int boneIndex, float[] transform16) : boolean` | function |
| `Moss_ModelSetMorphWeight` | `Model.setMorphWeight(...)` | `Model.setMorphWeight(int morphIndex, float weight) : boolean` | function |
| `Moss_RendererDrawMesh` | `Renderer.drawMesh(...)` | `Renderer.drawMesh(DrawMeshDesc desc) : void` | function |
| `Moss_RendererDrawModelDesc` | `Renderer.drawModelDesc(...)` | `Renderer.drawModelDesc(DrawModelDesc desc) : void` | function |
| `Moss_RendererDrawModel` | `Renderer.drawModel(...)` | `Renderer.drawModel(Model model, float[] transform) : void` | function |
| `Moss_RendererCreateFont` | `Renderer.createFont(...)` | `Renderer.createFont(FontDesc desc) : Font` | function |
| `Moss_RendererDestroyFont` | `Renderer.destroyFont(...)` | `Renderer.destroyFont(Font font) : void` | function |
| `Moss_FontLoad` | `Renderer.fontLoad(...)` | `Renderer.fontLoad(String path, float pixelSize) : Font` | function |
| `Moss_FontCreateFromMemory` | `Renderer.fontCreateFromMemory(...)` | `Renderer.fontCreateFromMemory(ByteBuffer data, long dataSize, float pixelSize) : Font` | function |
| `Moss_FontDestroy` | `Font.close(...)` | `Font.close() : void` | function |
| `Moss_FontGetMetrics` | `Font.getMetrics(...)` | `Font.getMetrics() : FontMetrics` | function |
| `Moss_FontGetGlyph` | `Font.getGlyph(...)` | `Font.getGlyph(int codepoint) : FontGlyph` | function |
| `Moss_FontGetAtlasPixels` | `Font.getAtlasPixels(...)` | `Font.getAtlasPixels(int[] width, int[] height, int[] stride) : UnsignedChar` | function |
| `Moss_FontGetAtlasTexture` | `Font.getAtlasTexture(...)` | `Font.getAtlasTexture() : Texture` | function |
| `Moss_FontUploadAtlas` | `Renderer.fontUploadAtlas(...)` | `Renderer.fontUploadAtlas(Font font) : boolean` | function |
| `Moss_FontMeasureText` | `Font.measureText(...)` | `Font.measureText(String text, float scale) : TextMeasure` | function |
| `Moss_FontBuildTextQuads` | `Renderer.fontBuildTextQuads(...)` | `Renderer.fontBuildTextQuads(DrawTextDesc desc, FontVertex outVertices, long vertexCapacity) : long` | function |
| `Moss_RendererDrawText` | `Renderer.drawText(...)` | `Renderer.drawText(DrawTextDesc desc) : void` | function |
| `Moss_RendererGetFontBackendMask` | `Renderer.getFontBackendMask(...)` | `Renderer.getFontBackendMask() : int` | function |
| `Moss_RendererBackendSupportsFonts` | `Renderer.backendSupportsFonts(...)` | `Renderer.backendSupportsFonts(GpuBackendType backend) : boolean` | function |
| `Moss_RendererDrawSprite` | `Renderer.drawSprite(...)` | `Renderer.drawSprite(SpriteDesc desc) : void` | function |
| `Moss_RendererCreateSpriteBatch` | `Renderer.createSpriteBatch(...)` | `Renderer.createSpriteBatch(int capacity) : SpriteBatch` | function |
| `Moss_RendererDestroySpriteBatch` | `Renderer.destroySpriteBatch(...)` | `Renderer.destroySpriteBatch(SpriteBatch batch) : void` | function |
| `Moss_SpriteBatchClear` | `SpriteBatch.clear(...)` | `SpriteBatch.clear() : void` | function |
| `Moss_SpriteBatchAdd` | `SpriteBatch.add(...)` | `SpriteBatch.add(SpriteDesc desc) : int` | function |
| `Moss_RendererDrawSpriteBatch` | `Renderer.drawSpriteBatch(...)` | `Renderer.drawSpriteBatch(SpriteBatch batch) : void` | function |
| `Moss_RendererCreateDebugDrawList` | `Renderer.createDebugDrawList(...)` | `Renderer.createDebugDrawList(int capacity) : DebugDrawList` | function |
| `Moss_RendererDestroyDebugDrawList` | `Renderer.destroyDebugDrawList(...)` | `Renderer.destroyDebugDrawList(DebugDrawList list) : void` | function |
| `Moss_DebugDrawListClear` | `DebugDrawList.clear(...)` | `DebugDrawList.clear() : void` | function |
| `Moss_DebugDrawLine` | `DebugDrawList.debugDrawLine(...)` | `DebugDrawList.debugDrawLine(DebugLineDesc desc) : int` | function |
| `Moss_DebugDrawBox` | `DebugDrawList.debugDrawBox(...)` | `DebugDrawList.debugDrawBox(DebugBoxDesc desc) : int` | function |
| `Moss_DebugDrawSphere` | `DebugDrawList.debugDrawSphere(...)` | `DebugDrawList.debugDrawSphere(DebugSphereDesc desc) : int` | function |
| `Moss_DebugDrawNavmesh` | `DebugDrawList.debugDrawNavmesh(...)` | `DebugDrawList.debugDrawNavmesh(DebugNavmeshDesc desc) : int` | function |
| `Moss_DebugDrawPhysicsContact` | `DebugDrawList.debugDrawPhysicsContact(...)` | `DebugDrawList.debugDrawPhysicsContact(DebugPhysicsContactDesc desc) : int` | function |
| `Moss_RendererDrawDebugList` | `Renderer.drawDebugList(...)` | `Renderer.drawDebugList(DebugDrawList list) : void` | function |
| `Moss_CameraCreate2D` | `Renderer.cameraCreateDimension2D(...)` | `Renderer.cameraCreateDimension2D(CameraDIMENSION2DDesc desc) : Camera` | function |
| `Moss_CameraCreate3D` | `Renderer.cameraCreateDimension3D(...)` | `Renderer.cameraCreateDimension3D(CameraDIMENSION3DDesc desc) : Camera` | function |
| `Moss_CameraCreateEditor` | `Renderer.cameraCreateEditor(...)` | `Renderer.cameraCreateEditor(EditorCameraDesc desc) : Camera` | function |
| `Moss_CameraDestroy` | `Camera.close(...)` | `Camera.close() : void` | function |
| `Moss_RendererSetCamera` | `Renderer.setCamera(...)` | `Renderer.setCamera(Camera camera) : void` | function |
| `Moss_RendererCreateShadowMap` | `Renderer.createShadowMap(...)` | `Renderer.createShadowMap(ShadowMapDesc desc) : ShadowMap` | function |
| `Moss_RendererDestroyShadowMap` | `Renderer.destroyShadowMap(...)` | `Renderer.destroyShadowMap(ShadowMap shadowMap) : void` | function |
| `Moss_RendererBeginShadowPass` | `Renderer.beginShadowPass(...)` | `Renderer.beginShadowPass(ShadowMap shadowMap) : void` | function |
| `Moss_RendererEndShadowPass` | `Renderer.endShadowPass(...)` | `Renderer.endShadowPass(ShadowMap shadowMap) : void` | function |
| `Moss_MeshDraw` | `Mesh.draw(...)` | `Mesh.draw() : void` | function |
| `Moss_ModelDraw` | `Model.draw(...)` | `Model.draw() : void` | function |
| `Moss_MeshRemove` | `Mesh.close(...)` | `Mesh.close() : void` | function |
| `Moss_ModelRemove` | `Model.close(...)` | `Model.close() : void` | function |
| `Moss_ResourceSetCreate` | `Renderer.createResourceSet(...)` | `Renderer.createResourceSet(ResourceSetLayoutDesc desc) : ResourceSet` | function |
| `Moss_ResourceSetDestroy` | `ResourceSet.close(...)` | `ResourceSet.close() : void` | function |
| `Moss_FramebufferCreate` | `Renderer.createFramebuffer(...)` | `Renderer.createFramebuffer(FramebufferDesc desc) : Framebuffer` | function |
| `Moss_FramebufferDestroy` | `Framebuffer.close(...)` | `Framebuffer.close() : void` | function |
| `Moss_RendererSetUpscaler` | `Renderer.setUpscaler(...)` | `Renderer.setUpscaler(Upscaler upscaler) : void` | function |
| `Moss_RendererSetUpscalerDesc` | `Renderer.setUpscalerDesc(...)` | `Renderer.setUpscalerDesc(RendererUpscalerDesc desc) : void` | function |
| `Moss_GPUGetPhysicalDeviceName` | `Renderer.getPhysicalDeviceName(...)` | `Renderer.getPhysicalDeviceName() : String` | function |
| `Moss_GPUAPIGetName` | `Renderer.gpuapiGetName(...)` | `Renderer.gpuapiGetName() : String` | function |
| `Moss_GPUAPIGetVersion` | `Renderer.gpuapiGetVersion(...)` | `Renderer.gpuapiGetVersion() : String` | function |
| `Moss_RendererDrawLine2D` | `Renderer.drawLineDimension2D(...)` | `Renderer.drawLineDimension2D(Vec2 from, Vec2 to, Color color, float thickness) : void` | function |
| `Moss_RendererDrawLines2D` | `Renderer.drawLinesDimension2D(...)` | `Renderer.drawLinesDimension2D(TArray<Vec2> positions, Color color, float thickness) : void` | function |
| `Moss_RendererDrawBox2D` | `Renderer.drawBoxDimension2D(...)` | `Renderer.drawBoxDimension2D(List<Vec2> center, float rotation, Color color) : void` | function |
| `Moss_RendererDrawCircle2D` | `Renderer.drawCircleDimension2D(...)` | `Renderer.drawCircleDimension2D(Vec2 center, float radius, Color color) : void` | function |
| `Moss_RendererDrawCylinder2D` | `Renderer.drawCylinderDimension2D(...)` | `Renderer.drawCylinderDimension2D(RMat44 matrix, float halfHeight, float radius, Color color) : void` | function |
| `Moss_RendererDrawTriangle2D` | `Renderer.drawTriangleDimension2D(...)` | `Renderer.drawTriangleDimension2D(Vec2 p0, Vec2 p1, Vec2 p2, Color color) : void` | function |
| `Moss_RendererDrawArrow2D` | `Renderer.drawArrowDimension2D(...)` | `Renderer.drawArrowDimension2D(Vec2 from, Vec2 to, Color color, float headSize) : void` | function |
| `Moss_RendererDrawMarker2D` | `Renderer.drawMarkerDimension2D(...)` | `Renderer.drawMarkerDimension2D(Vec2 position, Color color, float size) : void` | function |
| `Moss_RendererDrawBone2D` | `Renderer.drawBoneDimension2D(...)` | `Renderer.drawBoneDimension2D(RMat44 transform, float length, Color color) : void` | function |
| `Moss_RendererDrawCoordinateSystem2D` | `Renderer.drawCoordinateSystemDimension2D(...)` | `Renderer.drawCoordinateSystemDimension2D(RMat44 matrix, float size) : void` | function |
| `Moss_RendererDrawCapsule2D` | `Renderer.drawCapsuleDimension2D(...)` | `Renderer.drawCapsuleDimension2D(RMat44 matrix, float halfHeightOfCylinder, float radius, Color color) : void` | function |
| `Moss_RendererDrawConvex2D` | `Renderer.drawConvexDimension2D(...)` | `Renderer.drawConvexDimension2D(List<Vec2> points, Color color) : void` | function |
| `Moss_RendererDrawConvexHull2D` | `Renderer.drawConvexHullDimension2D(...)` | `Renderer.drawConvexHullDimension2D(List<Vec2> points, Color color) : void` | function |
| `Moss_RendererDrawPolygonShape2D` | `Renderer.drawPolygonShapeDimension2D(...)` | `Renderer.drawPolygonShapeDimension2D(List<Vec2> points, Color color) : void` | function |
| `Moss_RendererDrawGizmo2D` | `Renderer.drawGizmoDimension2D(...)` | `Renderer.drawGizmoDimension2D(RMat44 transform, float size) : void` | function |
| `Moss_RendererDrawLineBatch2D` | `Renderer.drawLineBatchDimension2D(...)` | `Renderer.drawLineBatchDimension2D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawBoxBatch2D` | `Renderer.drawBoxBatchDimension2D(...)` | `Renderer.drawBoxBatchDimension2D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawCircleBatch2D` | `Renderer.drawCircleBatchDimension2D(...)` | `Renderer.drawCircleBatchDimension2D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawArrowBatch2D` | `Renderer.drawArrowBatchDimension2D(...)` | `Renderer.drawArrowBatchDimension2D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawMarkerBatch2D` | `Renderer.drawMarkerBatchDimension2D(...)` | `Renderer.drawMarkerBatchDimension2D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawBoxInstanced2D` | `Renderer.drawBoxInstancedDimension2D(...)` | `Renderer.drawBoxInstancedDimension2D(Vec2 size, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawCircleInstanced2D` | `Renderer.drawCircleInstancedDimension2D(...)` | `Renderer.drawCircleInstancedDimension2D(float radius, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawCapsuleInstanced2D` | `Renderer.drawCapsuleInstancedDimension2D(...)` | `Renderer.drawCapsuleInstancedDimension2D(float halfHeightOfCylinder, float radius, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawMarkerInstanced2D` | `Renderer.drawMarkerInstancedDimension2D(...)` | `Renderer.drawMarkerInstancedDimension2D(float size, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawGizmoInstanced2D` | `Renderer.drawGizmoInstancedDimension2D(...)` | `Renderer.drawGizmoInstancedDimension2D(float size, RMat44 transform, int count) : void` | function |
| `Moss_RendererDrawLine3D` | `Renderer.drawLineDimension3D(...)` | `Renderer.drawLineDimension3D(Vec3 from, Vec3 to, Color color, float thickness) : void` | function |
| `Moss_RendererDrawLines3D` | `Renderer.drawLinesDimension3D(...)` | `Renderer.drawLinesDimension3D(TArray<Vec3> positions, Color color, float thickness) : void` | function |
| `Moss_RendererDrawBox3D` | `Renderer.drawBoxDimension3D(...)` | `Renderer.drawBoxDimension3D(List<Vec3> center, RMat44 rotation, Color color) : void` | function |
| `Moss_RendererDrawSphere3D` | `Renderer.drawSphereDimension3D(...)` | `Renderer.drawSphereDimension3D(Vec3 center, float radius, Color color) : void` | function |
| `Moss_RendererDrawCylinder3D` | `Renderer.drawCylinderDimension3D(...)` | `Renderer.drawCylinderDimension3D(Vec3 base, Vec3 top, float radius, Color color) : void` | function |
| `Moss_RendererDrawCone3D` | `Renderer.drawConeDimension3D(...)` | `Renderer.drawConeDimension3D(Vec3 base, Vec3 tip, float radius, Color color) : void` | function |
| `Moss_RendererDrawTriangle3D` | `Renderer.drawTriangleDimension3D(...)` | `Renderer.drawTriangleDimension3D(Vec3 p0, Vec3 p1, Vec3 p2, Color color) : void` | function |
| `Moss_RendererDrawArrow3D` | `Renderer.drawArrowDimension3D(...)` | `Renderer.drawArrowDimension3D(Vec3 from, Vec3 to, Color color, float headSize) : void` | function |
| `Moss_RendererDrawMarker3D` | `Renderer.drawMarkerDimension3D(...)` | `Renderer.drawMarkerDimension3D(Vec3 position, Color color, float size) : void` | function |
| `Moss_RendererDrawPlane3D` | `Renderer.drawPlaneDimension3D(...)` | `Renderer.drawPlaneDimension3D(Vec3 center, List<Vec3> normal, Color color) : void` | function |
| `Moss_RendererDrawBone3D` | `Renderer.drawBoneDimension3D(...)` | `Renderer.drawBoneDimension3D(RMat44 transform, float length, Color color) : void` | function |
| `Moss_RendererDrawCoordinateSystem3D` | `Renderer.drawCoordinateSystemDimension3D(...)` | `Renderer.drawCoordinateSystemDimension3D(RMat44 matrix, float size) : void` | function |
| `Moss_RendererDrawCapsule3D` | `Renderer.drawCapsuleDimension3D(...)` | `Renderer.drawCapsuleDimension3D(RMat44 matrix, float halfHeightOfCylinder, float radius, Color color) : void` | function |
| `Moss_RendererDrawConvex3D` | `Renderer.drawConvexDimension3D(...)` | `Renderer.drawConvexDimension3D(List<Vec3> points, Color color) : void` | function |
| `Moss_RendererDrawConvexHull3D` | `Renderer.drawConvexHullDimension3D(...)` | `Renderer.drawConvexHullDimension3D(List<Vec3> points, Color color) : void` | function |
| `Moss_RendererDrawPolygonShape3D` | `Renderer.drawPolygonShapeDimension3D(...)` | `Renderer.drawPolygonShapeDimension3D(List<Vec3> points, Color color) : void` | function |
| `Moss_RendererDrawGizmo3D` | `Renderer.drawGizmoDimension3D(...)` | `Renderer.drawGizmoDimension3D(RMat44 transform, float size) : void` | function |
| `Moss_RendererDrawLineBatch3D` | `Renderer.drawLineBatchDimension3D(...)` | `Renderer.drawLineBatchDimension3D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawBoxBatch3D` | `Renderer.drawBoxBatchDimension3D(...)` | `Renderer.drawBoxBatchDimension3D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawSphereBatch3D` | `Renderer.drawSphereBatchDimension3D(...)` | `Renderer.drawSphereBatchDimension3D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawArrowBatch3D` | `Renderer.drawArrowBatchDimension3D(...)` | `Renderer.drawArrowBatchDimension3D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawMarkerBatch3D` | `Renderer.drawMarkerBatchDimension3D(...)` | `Renderer.drawMarkerBatchDimension3D(RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawBoxInstanced3D` | `Renderer.drawBoxInstancedDimension3D(...)` | `Renderer.drawBoxInstancedDimension3D(Vec3 size, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawSphereInstanced3D` | `Renderer.drawSphereInstancedDimension3D(...)` | `Renderer.drawSphereInstancedDimension3D(float radius, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawCapsuleInstanced3D` | `Renderer.drawCapsuleInstancedDimension3D(...)` | `Renderer.drawCapsuleInstancedDimension3D(float halfHeightOfCylinder, float radius, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawCylinderInstanced3D` | `Renderer.drawCylinderInstancedDimension3D(...)` | `Renderer.drawCylinderInstancedDimension3D(float halfHeight, float radius, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawConeInstanced3D` | `Renderer.drawConeInstancedDimension3D(...)` | `Renderer.drawConeInstancedDimension3D(float height, float radius, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawMarkerInstanced3D` | `Renderer.drawMarkerInstancedDimension3D(...)` | `Renderer.drawMarkerInstancedDimension3D(float size, RMat44 transform, Color colour, int count) : void` | function |
| `Moss_RendererDrawGizmoInstanced3D` | `Renderer.drawGizmoInstancedDimension3D(...)` | `Renderer.drawGizmoInstancedDimension3D(float size, RMat44 transform, int count) : void` | function |
| `Moss_VulkanGetDevice` | `Renderer.getDevice(...)` | `Renderer.getDevice() : VkDevice` | function |
| `Moss_VulkanGetDescriptorPool` | `Renderer.getDescriptorPool(...)` | `Renderer.getDescriptorPool() : VkDescriptorPool` | function |
| `Moss_VulkanGetDescriptorSetLayoutTexture` | `Renderer.getDescriptorSetLayoutTexture(...)` | `Renderer.getDescriptorSetLayoutTexture() : VkDescriptorSetLayout` | function |
| `Moss_VulkanGetTextureSamplerRepeat` | `Renderer.getTextureSamplerRepeat(...)` | `Renderer.getTextureSamplerRepeat() : VkSampler` | function |
| `Moss_VulkanGetTextureSamplerShadow` | `Renderer.getTextureSamplerShadow(...)` | `Renderer.getTextureSamplerShadow() : VkSampler` | function |
| `Moss_VulkanGetRenderPassShadow` | `Renderer.getRenderPassShadow(...)` | `Renderer.getRenderPassShadow() : VkRenderPass` | function |
| `Moss_VulkanGetRenderPass` | `Renderer.getRenderPass(...)` | `Renderer.getRenderPass() : VkRenderPass` | function |
| `Moss_VulkanGetPipelineLayout` | `Renderer.getPipelineLayout(...)` | `Renderer.getPipelineLayout() : VkPipelineLayout` | function |
| `Moss_VulkanGetCommandBuffer` | `Renderer.getCommandBuffer(...)` | `Renderer.getCommandBuffer() : VkCommandBuffer` | function |
| `Moss_VulkanStartTempCommandBuffer` | `Renderer.startTempCommandBuffer(...)` | `Renderer.startTempCommandBuffer() : VkCommandBuffer` | function |
| `Moss_VulkanEndTempCommandBuffer` | `Renderer.endTempCommandBuffer(...)` | `Renderer.endTempCommandBuffer(VkCommandBuffer commandBuffer) : void` | function |
| `Moss_VulkanAllocateMemory` | `Renderer.allocateMemory(...)` | `Renderer.allocateMemory(VkDeviceSize size, int memoryTypeBits, VkMemoryPropertyFlags properties, VkDeviceMemory outMemory) : void` | function |
| `Moss_VulkanFreeMemory` | `Renderer.freeMemory(...)` | `Renderer.freeMemory(VkDeviceMemory memory, VkDeviceSize size) : void` | function |
| `Moss_VulkanCreateBuffer` | `Renderer.createBuffer(...)` | `Renderer.createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferVK outBuffer) : void` | function |
| `Moss_VulkanCopyBuffer` | `Renderer.copyBuffer(...)` | `Renderer.copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) : void` | function |
| `Moss_VulkanCreateDeviceLocalBuffer` | `Renderer.createDeviceLocalBuffer(...)` | `Renderer.createDeviceLocalBuffer(ByteBuffer data, VkDeviceSize size, VkBufferUsageFlags usage, BufferVK outBuffer) : void` | function |
| `Moss_VulkanFreeBuffer` | `Renderer.freeBuffer(...)` | `Renderer.freeBuffer(BufferVK buffer) : void` | function |
| `Moss_VulkanCreateConstantBuffer` | `Renderer.createConstantBuffer(...)` | `Renderer.createConstantBuffer(VkDeviceSize bufferSize) : ConstantBufferVK` | function |
| `Moss_VulkanCreateImage` | `Renderer.createImage(...)` | `Renderer.createImage(int width, int height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage outImage, VkDeviceMemory outMemory) : void` | function |
| `Moss_VulkanDestroyImage` | `Renderer.destroyImage(...)` | `Renderer.destroyImage(VkImage image, VkDeviceMemory memory) : void` | function |
| `Moss_VulkanCreateImageView` | `Renderer.createImageView(...)` | `Renderer.createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) : VkImageView` | function |
| `Moss_VulkanFindDepthFormat` | `Renderer.findDepthFormat(...)` | `Renderer.findDepthFormat() : VkFormat` | function |
| `Moss_Vulkan_EncodeSPIRV` | `Renderer.encodeSpirv(...)` | `Renderer.encodeSpirv(String path) : void` | function |
| `Moss_Vulkan_DecodeSPIRV` | `Renderer.decodeSpirv(...)` | `Renderer.decodeSpirv(String path) : void` | function |
| `Moss_DX12GetDevice` | `Renderer.dx12GetDevice(...)` | `Renderer.dx12GetDevice() : IdDIMENSION3D1DIMENSION2Device` | function |
| `Moss_DX12GetRootSignature` | `Renderer.dx12GetRootSignature(...)` | `Renderer.dx12GetRootSignature() : IdDIMENSION3D12RootSignature` | function |
| `Moss_DX12GetCommandList` | `Renderer.dx12GetCommandList(...)` | `Renderer.dx12GetCommandList() : IdDIMENSION3D12GraphicsCommandList` | function |
| `Moss_DX12GetUploadQueue` | `Renderer.dx12GetUploadQueue(...)` | `Renderer.dx12GetUploadQueue() : CommandQueueDX12` | function |
| `Moss_DX12GetDSVHeap` | `Renderer.dx12GetDsvHeap(...)` | `Renderer.dx12GetDsvHeap() : DescriptorHeapDX12` | function |
| `Moss_DX12GetSRVHeap` | `Renderer.dx12GetSrvHeap(...)` | `Renderer.dx12GetSrvHeap() : DescriptorHeapDX12` | function |
| `Moss_DX_CompileHLSL` | `Renderer.compileHlsl(...)` | `Renderer.compileHlsl(String inputPath, String outputPath, String entryPoint, String target) : boolean` | function |
| `Moss_DX_EnableDebugLayer` | `Renderer.enableDebugLayer(...)` | `Renderer.enableDebugLayer(boolean enable) : void` | function |
| `Moss_DX_GetShaderModel` | `Renderer.getShaderModel(...)` | `Renderer.getShaderModel() : int` | function |
| `Moss_MetalGetView` | `Renderer.getView(...)` | `Renderer.getView() : MTKView` | function |
| `Moss_MetalGetDevice` | `Renderer.getDevice(...)` | `Renderer.getDevice() : ByteBuffer` | function |
| `Moss_MetalGetRenderEncoder` | `Renderer.getRenderEncoder(...)` | `Renderer.getRenderEncoder() : ByteBuffer` | function |
| `Moss_Metal_CompileMSL` | `Renderer.compileMsl(...)` | `Renderer.compileMsl(String inputPath, String outputPath) : boolean` | function |
| `Moss_Metal_SupportsFamily` | `Renderer.supportsFamily(...)` | `Renderer.supportsFamily(int family) : boolean` | function |

### `Moss_stdinc.h` → `moss.core`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Allocate` | `AllocatorCallbacks.allocate` | `@FunctionalInterface interface Allocate` | callback |
| `Reallocate` | `AllocatorCallbacks.reallocate` | `@FunctionalInterface interface Reallocate` | callback |
| `Free` | `AllocatorCallbacks.free` | `@FunctionalInterface interface Free` | callback |
| `AlignedAllocate` | `AllocatorCallbacks.alignedAllocate` | `@FunctionalInterface interface AlignedAllocate` | callback |
| `AlignedFree` | `AllocatorCallbacks.alignedFree` | `@FunctionalInterface interface AlignedFree` | callback |
| `RegisterDefaultAllocator` | `Core.registerDefaultAllocator(...)` | `Core.registerDefaultAllocator() : void` | function |
| `Allocate` | `Core.allocate(...)` | `Core.allocate(long inSize) : ByteBuffer` | function |
| `Reallocate` | `Core.reallocate(...)` | `Core.reallocate(ByteBuffer inBlock, long inOldSize, long inNewSize) : ByteBuffer` | function |
| `Free` | `Core.free(...)` | `Core.free(ByteBuffer inBlock) : void` | function |
| `AlignedAllocate` | `Core.alignedAllocate(...)` | `Core.alignedAllocate(long inSize, long inAlignment) : ByteBuffer` | function |
| `AlignedFree` | `Core.alignedFree(...)` | `Core.alignedFree(ByteBuffer inBlock) : void` | function |
| `Moss_rand` | `Core.rand(...)` | `Core.rand(Int32 n) : Int32` | function |
| `Moss_randf` | `Core.randf(...)` | `Core.randf() : float` | function |
| `Moss_randi` | `Core.randi(...)` | `Core.randi() : int` | function |
| `Moss_srand` | `Core.srand(...)` | `Core.srand(Int64 seed) : void` | function |
| `Moss_randi_range` | `Core.randiRange(...)` | `Core.randiRange(Int32 from, Int32 to) : Int32` | function |
| `Moss_randf_range` | `Core.randfRange(...)` | `Core.randfRange(float from, float to) : float` | function |
| `Moss_rand_gaussian` | `Core.randGaussian(...)` | `Core.randGaussian(float mean, float stddev) : float` | function |
| `Moss_rand_unit` | `Core.randUnit(...)` | `Core.randUnit() : float` | function |
| `Moss_GetTicks` | `Core.getTicks(...)` | `Core.getTicks() : Time` | function |
| `Moss_GetSeconds` | `Core.getSeconds(...)` | `Core.getSeconds(Time time) : double` | function |
| `Moss_GetMilliseconds` | `Core.getMilliseconds(...)` | `Core.getMilliseconds(Time ticks) : float` | function |
| `Moss_GetMillisecondsAndReset` | `Core.getMillisecondsAndReset(...)` | `Core.getMillisecondsAndReset(Time ticks) : float` | function |
| `Moss_Yield` | `Core.yield(...)` | `Core.yield() : void` | function |
| `Moss_Delay` | `Core.delay(...)` | `Core.delay(double delay) : double` | function |
| `Moss_LocalTime` | `Core.localTime(...)` | `Core.localTime() : String` | function |
| `Moss_TimeStamp` | `Core.timeStamp(...)` | `Core.timeStamp() : String` | function |
| `Moss_TimeNow` | `Core.timeNow(...)` | `Core.timeNow() : String` | function |
| `Moss_CTimeNow` | `Core.cTimeNow(...)` | `Core.cTimeNow() : String` | function |
| `Moss_TimeStampET` | `Core.timeStampEt(...)` | `Core.timeStampEt() : String` | function |
| `Moss_TimeStampCT` | `Core.timeStampCt(...)` | `Core.timeStampCt() : String` | function |
| `Moss_TimeStampMT` | `Core.timeStampMt(...)` | `Core.timeStampMt() : String` | function |
| `Moss_TimeStampPT` | `Core.timeStampPt(...)` | `Core.timeStampPt() : String` | function |
| `Moss_FormatTime` | `Core.formatTime(...)` | `Core.formatTime(String fmt) : String` | function |

### `Moss_XR.h` → `moss.xr`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_XR_Initialize` | `XR.initialize(...)` | `XR.initialize() : boolean` | function |
| `Moss_XR_Shutdown` | `XR.shutdown(...)` | `XR.shutdown() : void` | function |
| `Moss_XR_GetCapabilities` | `XR.getCapabilities(...)` | `XR.getCapabilities() : XrCapabilities` | function |
| `Moss_XR_EnableValidationLayers` | `XR.enableValidationLayers(...)` | `XR.enableValidationLayers() : boolean` | function |
| `Moss_XR_CreateSession` | `XR.createSession(...)` | `XR.createSession() : XrSession` | function |
| `Moss_XR_DestroySession` | `XrSession.close(...)` | `XrSession.close() : void` | function |
| `Moss_XR_GetSessionState` | `XrSession.getState(...)` | `XrSession.getState() : XrSessionState` | function |
| `Moss_XR_BeginFrame` | `XrSession.beginFrame(...)` | `XrSession.beginFrame() : boolean` | function |
| `Moss_XR_EndFrame` | `XrSession.endFrame(...)` | `XrSession.endFrame() : void` | function |
| `Moss_XR_GetPredictedDisplayTime` | `XrSession.getPredictedDisplayTime(...)` | `XrSession.getPredictedDisplayTime() : XrTime` | function |
| `Moss_XR_GetDeltaSeconds` | `XR.getDeltaSeconds(...)` | `XR.getDeltaSeconds() : float` | function |
| `Moss_XR_HandleEvents` | `XrSession.handleEvents(...)` | `XrSession.handleEvents() : void` | function |
| `Moss_XR_GetViewCount` | `XrSession.getViewCount(...)` | `XrSession.getViewCount() : int` | function |
| `Moss_XR_GetView` | `XrSession.getView(...)` | `XrSession.getView(int index, XrView outView) : boolean` | function |
| `Moss_XR_CreateActionSet` | `XR.createActionSet(...)` | `XR.createActionSet(String name) : XrActionSet` | function |
| `Moss_XR_DestroyActionSet` | `XrActionSet.close(...)` | `XrActionSet.close() : void` | function |
| `Moss_XR_CreateAction` | `XrActionSet.createAction(...)` | `XrActionSet.createAction(String name, XrActionType type) : XrAction` | function |
| `Moss_XR_DestroyAction` | `XrAction.close(...)` | `XrAction.close() : void` | function |
| `Moss_XR_AttachActionSet` | `XrActionSet.attach(...)` | `XrActionSet.attach(XrSession session) : void` | function |
| `Moss_XR_SyncActions` | `XrSession.syncActions(...)` | `XrSession.syncActions() : void` | function |
| `Moss_XR_GetActionBoolean` | `XrAction.getBoolean(...)` | `XrAction.getBoolean() : boolean` | function |
| `Moss_XR_GetActionFloat` | `XrAction.getFloat(...)` | `XrAction.getFloat() : float` | function |
| `Moss_XR_GetActionPose` | `XrAction.getPose(...)` | `XrAction.getPose(XrPose outPose) : boolean` | function |
| `Moss_XR_StopHaptic` | `XrAction.stopHaptic(...)` | `XrAction.stopHaptic() : void` | function |
| `Moss_XR_CreateHand` | `XR.createHand(...)` | `XR.createHand() : XrHandModifier` | function |
| `Moss_XR_GetHandJointPose` | `XrHandModifier.getHandJointPose(...)` | `XrHandModifier.getHandJointPose(XrHandJoint joint, XrPose outPose) : boolean` | function |
| `Moss_XR_DestroyHand` | `XrHandModifier.destroyHand(...)` | `XrHandModifier.destroyHand() : void` | function |
| `Moss_XR_GetHandPose` | `XrHandModifier.getHandPose(...)` | `XrHandModifier.getHandPose(XrPose outPose) : boolean` | function |
| `Moss_XR_CreateBody` | `XR.createBody(...)` | `XR.createBody() : XrBodyModifier` | function |
| `Moss_XR_GetBodyJointPose` | `XrBodyModifier.getBodyJointPose(...)` | `XrBodyModifier.getBodyJointPose(XrBodyJoint joint, XrPose outPose) : boolean` | function |
| `Moss_XR_DestroyBody` | `XrBodyModifier.destroyBody(...)` | `XrBodyModifier.destroyBody() : void` | function |
| `Moss_XR_CreateFace` | `XR.createFace(...)` | `XR.createFace() : XrFaceModifier` | function |
| `Moss_XR_DestroyFace` | `XrFaceModifier.destroyFace(...)` | `XrFaceModifier.destroyFace() : void` | function |
| `Moss_XR_UpdateBodyTracking` | `XrSession.updateBodyTracking(...)` | `XrSession.updateBodyTracking() : void` | function |
| `Moss_XR_UpdateHandTracking` | `XrSession.updateHandTracking(...)` | `XrSession.updateHandTracking() : void` | function |
| `Moss_XR_EnablePassthrough` | `XR.enablePassthrough(...)` | `XR.enablePassthrough(boolean enable) : boolean` | function |
| `Moss_XR_EnableFoveatedRendering` | `XR.enableFoveatedRendering(...)` | `XR.enableFoveatedRendering(boolean enable) : boolean` | function |
| `Moss_XR_GetBackendName` | `XR.getBackendName(...)` | `XR.getBackendName() : String` | function |
| `Moss_XR_GetBackendVersion` | `XR.getBackendVersion(...)` | `XR.getBackendVersion() : int` | function |
| `Moss_XR_BeginDebugLabel` | `XR.beginDebugLabel(...)` | `XR.beginDebugLabel(String label) : void` | function |
| `Moss_XR_EndDebugLabel` | `XR.endDebugLabel(...)` | `XR.endDebugLabel() : void` | function |


## 8. Native header and feature coverage

The public binding must cover the complete Moss header tree, not only the exported function index. Many Core, Geometry, Physics, Renderer, Resources, Skeleton, TriangleSplitter and Variants APIs are class members, templates, inline methods, or declarations not represented by the top-level free-function catalogue.

## Complete header inventory

Each link opens the corresponding source file in `include/Moss/`. The rightmost count shows declarations beginning with `MOSS_API` in that header; zero does not mean the header has no public API (it may expose class members, templates, inline methods, typedefs, or preprocessor declarations).

### Core (40 headers)

Foundational C++ utilities.

| Header | Export-marked declarations |
|---|---:|
| [`Core/BinaryHeap.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/BinaryHeap.h) | 0 |
| [`Core/ByteBuffer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/ByteBuffer.h) | 0 |
| [`Core/FPControlWord.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FPControlWord.h) | 0 |
| [`Core/FPException.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FPException.h) | 0 |
| [`Core/FPFlushDenormals.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FPFlushDenormals.h) | 0 |
| [`Core/Factory.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Factory.h) | 0 |
| [`Core/FixedSizeFreeList.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FixedSizeFreeList.h) | 0 |
| [`Core/FreeBlockAlloc.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FreeBlockAlloc.h) | 0 |
| [`Core/HashCombine.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/HashCombine.h) | 0 |
| [`Core/HashTable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/HashTable.h) | 0 |
| [`Core/InsertionSort.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/InsertionSort.h) | 0 |
| [`Core/JobSystem.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystem.h) | 0 |
| [`Core/JobSystemSingleThreaded.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystemSingleThreaded.h) | 0 |
| [`Core/JobSystemThreadPool.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystemThreadPool.h) | 0 |
| [`Core/JobSystemWithBarrier.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystemWithBarrier.h) | 0 |
| [`Core/LinearCurve.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/LinearCurve.h) | 0 |
| [`Core/LockFreeHashMap.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/LockFreeHashMap.h) | 0 |
| [`Core/Memory.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Memory.h) | 11 |
| [`Core/Mutex.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Mutex.h) | 0 |
| [`Core/MutexArray.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/MutexArray.h) | 0 |
| [`Core/NonCopyable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/NonCopyable.h) | 0 |
| [`Core/Profiler.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Profiler.h) | 4 |
| [`Core/QuickSort.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/QuickSort.h) | 0 |
| [`Core/RTTI.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/RTTI.h) | 0 |
| [`Core/Reference.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Reference.h) | 0 |
| [`Core/Result.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Result.h) | 0 |
| [`Core/STLAlignedAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLAlignedAllocator.h) | 0 |
| [`Core/STLAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLAllocator.h) | 0 |
| [`Core/STLLocalAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLLocalAllocator.h) | 0 |
| [`Core/STLTempAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLTempAllocator.h) | 0 |
| [`Core/ScopeExit.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/ScopeExit.h) | 0 |
| [`Core/Semaphore.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Semaphore.h) | 0 |
| [`Core/StreamIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamIn.h) | 0 |
| [`Core/StreamOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamOut.h) | 0 |
| [`Core/StreamUtils.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamUtils.h) | 0 |
| [`Core/StreamWrapper.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamWrapper.h) | 0 |
| [`Core/StridedPtr.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StridedPtr.h) | 0 |
| [`Core/StringTools.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StringTools.h) | 6 |
| [`Core/TempAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/TempAllocator.h) | 0 |
| [`Core/TickCounter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/TickCounter.h) | 0 |

### Geometry (9 headers)

Geometry algorithms and primitives.

| Header | Export-marked declarations |
|---|---:|
| [`Geometry/ClipPoly.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ClipPoly.h) | 0 |
| [`Geometry/ClosestPoint.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ClosestPoint.h) | 0 |
| [`Geometry/ConvexHullBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ConvexHullBuilder.h) | 0 |
| [`Geometry/ConvexSupport.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ConvexSupport.h) | 0 |
| [`Geometry/EPAConvexHullBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/EPAConvexHullBuilder.h) | 0 |
| [`Geometry/EPAPenetrationDepth.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/EPAPenetrationDepth.h) | 0 |
| [`Geometry/GJKClosestPoint.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/GJKClosestPoint.h) | 0 |
| [`Geometry/RayCylinder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/RayCylinder.h) | 0 |
| [`Geometry/Triangle.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/Triangle.h) | 0 |

### Physics (69 headers)

Simulation, body, shape, collision, constraint and query implementation headers.

| Header | Export-marked declarations |
|---|---:|
| [`Physics/AABBTree/AABBTreeBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/AABBTreeBuilder.h) | 0 |
| [`Physics/AABBTree/AABBTreeToBuffer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/AABBTreeToBuffer.h) | 0 |
| [`Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat.h) | 0 |
| [`Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat2D.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat2D.h) | 0 |
| [`Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags.h) | 0 |
| [`Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags2D.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags2D.h) | 0 |
| [`Physics/Body/Body.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Body/Body.h) | 1 |
| [`Physics/Body/BodyID.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Body/BodyID.h) | 0 |
| [`Physics/Character/Character.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Character/Character.h) | 0 |
| [`Physics/Cloth/Cloth.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Cloth/Cloth.h) | 0 |
| [`Physics/Collision/ActiveEdges.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ActiveEdges.h) | 0 |
| [`Physics/Collision/BroadPhase/BroadPhase.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/BroadPhase/BroadPhase.h) | 0 |
| [`Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h) | 0 |
| [`Physics/Collision/BroadPhase/QuadTree.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/BroadPhase/QuadTree.h) | 0 |
| [`Physics/Collision/CastConvexVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CastConvexVsTriangles.h) | 0 |
| [`Physics/Collision/CastResult.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CastResult.h) | 0 |
| [`Physics/Collision/CastSphereVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CastSphereVsTriangles.h) | 0 |
| [`Physics/Collision/CollideConvexVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideConvexVsTriangles.h) | 0 |
| [`Physics/Collision/CollidePointResult.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollidePointResult.h) | 0 |
| [`Physics/Collision/CollideShape.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideShape.h) | 0 |
| [`Physics/Collision/CollideShapeVsShapePerLeaf.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideShapeVsShapePerLeaf.h) | 0 |
| [`Physics/Collision/CollideSoftBodyVertexIterator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideSoftBodyVertexIterator.h) | 0 |
| [`Physics/Collision/CollideSoftBodyVerticesVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideSoftBodyVerticesVsTriangles.h) | 0 |
| [`Physics/Collision/CollideSphereVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideSphereVsTriangles.h) | 0 |
| [`Physics/Collision/CollisionCollector.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionCollector.h) | 0 |
| [`Physics/Collision/CollisionCollectorImpl.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionCollectorImpl.h) | 0 |
| [`Physics/Collision/CollisionDispatch.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionDispatch.h) | 0 |
| [`Physics/Collision/CollisionGroup.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionGroup.h) | 0 |
| [`Physics/Collision/ContactListener.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ContactListener.h) | 0 |
| [`Physics/Collision/EstimateCollisionResponse.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/EstimateCollisionResponse.h) | 1 |
| [`Physics/Collision/GroupFilter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/GroupFilter.h) | 0 |
| [`Physics/Collision/GroupFilterTable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/GroupFilterTable.h) | 0 |
| [`Physics/Collision/InternalEdgeRemovingCollector.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/InternalEdgeRemovingCollector.h) | 0 |
| [`Physics/Collision/ManifoldBetweenTwoFaces.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ManifoldBetweenTwoFaces.h) | 2 |
| [`Physics/Collision/NarrowPhaseQuery.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/NarrowPhaseQuery.h) | 0 |
| [`Physics/Collision/NarrowPhaseStats.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/NarrowPhaseStats.h) | 0 |
| [`Physics/Collision/ObjectLayer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ObjectLayer.h) | 0 |
| [`Physics/Collision/ObjectLayerPairFilterMask.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ObjectLayerPairFilterMask.h) | 0 |
| [`Physics/Collision/ObjectLayerPairFilterTable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ObjectLayerPairFilterTable.h) | 0 |
| [`Physics/Collision/PhysicsMaterial.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/PhysicsMaterial.h) | 0 |
| [`Physics/Collision/PhysicsMaterialSimple.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/PhysicsMaterialSimple.h) | 0 |
| [`Physics/Collision/RayCast.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/RayCast.h) | 0 |
| [`Physics/Collision/Shape/CompoundShapeVisitors.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/CompoundShapeVisitors.h) | 0 |
| [`Physics/Collision/Shape/ConvexHullShape.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/ConvexHullShape.h) | 0 |
| [`Physics/Collision/Shape/GetTrianglesContext.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/GetTrianglesContext.h) | 0 |
| [`Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h) | 0 |
| [`Physics/Collision/Shape/Shapes.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/Shapes.h) | 0 |
| [`Physics/Collision/Shape/SubShapeID.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/SubShapeID.h) | 0 |
| [`Physics/Collision/Shape/SubShapeIDPair.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/SubShapeIDPair.h) | 0 |
| [`Physics/Collision/ShapeCast.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ShapeCast.h) | 0 |
| [`Physics/Collision/ShapeFilter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ShapeFilter.h) | 0 |
| [`Physics/Collision/SimShapeFilter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/SimShapeFilter.h) | 0 |
| [`Physics/Collision/SimShapeFilterWrapper.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/SimShapeFilterWrapper.h) | 0 |
| [`Physics/Collision/SortReverseAndStore.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/SortReverseAndStore.h) | 0 |
| [`Physics/Collision/TransformedShape.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/TransformedShape.h) | 0 |
| [`Physics/Constraints/ConstraintManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Constraints/ConstraintManager.h) | 0 |
| [`Physics/Constraints/Constraints.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Constraints/Constraints.h) | 0 |
| [`Physics/Hair/Hair.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Hair/Hair.h) | 0 |
| [`Physics/IslandBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/IslandBuilder.h) | 0 |
| [`Physics/LargeIslandSplitter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/LargeIslandSplitter.h) | 0 |
| [`Physics/PhysicsLock.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsLock.h) | 0 |
| [`Physics/PhysicsScene.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsScene.h) | 0 |
| [`Physics/PhysicsSystem.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsSystem.h) | 0 |
| [`Physics/PhysicsUpdateContext.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsUpdateContext.h) | 0 |
| [`Physics/Ragdoll/Ragdoll.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Ragdoll/Ragdoll.h) | 0 |
| [`Physics/SoftBody/Softbody.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/SoftBody/Softbody.h) | 0 |
| [`Physics/StateRecorder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/StateRecorder.h) | 0 |
| [`Physics/StateRecorderImpl.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/StateRecorderImpl.h) | 0 |
| [`Physics/Vehicle/Vehicle.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Vehicle/Vehicle.h) | 0 |

### Renderer (65 headers)

Renderer resources and graphics backend headers.

| Header | Export-marked declarations |
|---|---:|
| [`Renderer/DX12/CommandQueueDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/CommandQueueDX12.h) | 0 |
| [`Renderer/DX12/ConstantBufferDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/ConstantBufferDX12.h) | 0 |
| [`Renderer/DX12/DescriptorHeapDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/DescriptorHeapDX12.h) | 0 |
| [`Renderer/DX12/LightingDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/LightingDX12.h) | 0 |
| [`Renderer/DX12/PipelineStateDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/PipelineStateDX12.h) | 0 |
| [`Renderer/DX12/RenderInstancesDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/RenderInstancesDX12.h) | 0 |
| [`Renderer/DX12/RenderPrimitiveDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/RenderPrimitiveDX12.h) | 0 |
| [`Renderer/DX12/RendererDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/RendererDX12.h) | 0 |
| [`Renderer/DX12/ShaderDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/ShaderDX12.h) | 0 |
| [`Renderer/DX12/TextureDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/TextureDX12.h) | 0 |
| [`Renderer/DebugRenderer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRenderer.h) | 0 |
| [`Renderer/DebugRendererPlayback.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRendererPlayback.h) | 0 |
| [`Renderer/DebugRendererRecorder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRendererRecorder.h) | 0 |
| [`Renderer/DebugRendererSimple.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRendererSimple.h) | 0 |
| [`Renderer/FogVolume.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/FogVolume.h) | 0 |
| [`Renderer/Font.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Font.h) | 0 |
| [`Renderer/GL/FontGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/FontGL.h) | 0 |
| [`Renderer/GL/LightingGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/LightingGL.h) | 0 |
| [`Renderer/GL/MeshGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/MeshGL.h) | 0 |
| [`Renderer/GL/PipelineStateGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/PipelineStateGL.h) | 0 |
| [`Renderer/GL/PostProcess.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/PostProcess.h) | 0 |
| [`Renderer/GL/ReflectionProbe.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/ReflectionProbe.h) | 0 |
| [`Renderer/GL/Renderer_GL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/Renderer_GL.h) | 0 |
| [`Renderer/GL/ShaderGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/ShaderGL.h) | 0 |
| [`Renderer/GL/StorageBufferGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/StorageBufferGL.h) | 0 |
| [`Renderer/GL/SubViewportGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/SubViewportGL.h) | 0 |
| [`Renderer/GL/SurfaceGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/SurfaceGL.h) | 0 |
| [`Renderer/GL/TextureGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/TextureGL.h) | 0 |
| [`Renderer/GL/glad.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/glad.h) | 0 |
| [`Renderer/MTL/PipelineStateMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/PipelineStateMTL.h) | 0 |
| [`Renderer/MTL/RenderInstancesMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/RenderInstancesMTL.h) | 0 |
| [`Renderer/MTL/RenderPrimitiveMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/RenderPrimitiveMTL.h) | 0 |
| [`Renderer/MTL/RendererMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/RendererMTL.h) | 0 |
| [`Renderer/MTL/ShaderMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/ShaderMTL.h) | 0 |
| [`Renderer/MTL/TextureMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/TextureMTL.h) | 0 |
| [`Renderer/Managers/DecalManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/DecalManager.h) | 0 |
| [`Renderer/Managers/FontManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/FontManager.h) | 0 |
| [`Renderer/Managers/LightingManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/LightingManager.h) | 0 |
| [`Renderer/Managers/MeshManager3D.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/MeshManager3D.h) | 0 |
| [`Renderer/Managers/ParticleEffectManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/ParticleEffectManager.h) | 0 |
| [`Renderer/Managers/SurfaceManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/SurfaceManager.h) | 0 |
| [`Renderer/Material2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Material2.h) | 0 |
| [`Renderer/Material3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Material3.h) | 0 |
| [`Renderer/Mesh.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Mesh.h) | 0 |
| [`Renderer/Model.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Model.h) | 0 |
| [`Renderer/PipelineState.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/PipelineState.h) | 0 |
| [`Renderer/Renderer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Renderer.h) | 0 |
| [`Renderer/Shader.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Shader.h) | 0 |
| [`Renderer/SkyBox.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/SkyBox.h) | 0 |
| [`Renderer/StorageBuffer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/StorageBuffer.h) | 0 |
| [`Renderer/SubViewport.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/SubViewport.h) | 0 |
| [`Renderer/Surface.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Surface.h) | 0 |
| [`Renderer/Texture.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Texture.h) | 0 |
| [`Renderer/VK/ConstantBufferVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/ConstantBufferVK.h) | 0 |
| [`Renderer/VK/LightingManagerVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/LightingManagerVK.h) | 0 |
| [`Renderer/VK/MeshVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/MeshVK.h) | 0 |
| [`Renderer/VK/PipelineStateVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/PipelineStateVK.h) | 0 |
| [`Renderer/VK/RenderInstancesVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/RenderInstancesVK.h) | 0 |
| [`Renderer/VK/RenderPrimitiveVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/RenderPrimitiveVK.h) | 0 |
| [`Renderer/VK/RendererVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/RendererVK.h) | 0 |
| [`Renderer/VK/Renderer_VK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/Renderer_VK.h) | 4 |
| [`Renderer/VK/ShaderVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/ShaderVK.h) | 0 |
| [`Renderer/VK/StorageBufferVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/StorageBufferVK.h) | 0 |
| [`Renderer/VK/SubViewportVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/SubViewportVK.h) | 0 |
| [`Renderer/VK/TextureVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/TextureVK.h) | 0 |

### Resources (21 headers)

File/resource and serialization headers.

| Header | Export-marked declarations |
|---|---:|
| [`Resources/BezierCurve.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/BezierCurve.h) | 0 |
| [`Resources/Config.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Config.h) | 0 |
| [`Resources/Gradient.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Gradient.h) | 0 |
| [`Resources/Json.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Json.h) | 0 |
| [`Resources/ObjectStream/GetPrimitiveTypeOfType.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/GetPrimitiveTypeOfType.h) | 0 |
| [`Resources/ObjectStream/ObjectStream.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStream.h) | 4 |
| [`Resources/ObjectStream/ObjectStreamBinaryIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamBinaryIn.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamBinaryOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamBinaryOut.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamIn.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamOut.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamTextIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamTextIn.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamTextOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamTextOut.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamTypes.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamTypes.h) | 0 |
| [`Resources/ObjectStream/SerializableAttribute.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableAttribute.h) | 0 |
| [`Resources/ObjectStream/SerializableAttributeEnum.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableAttributeEnum.h) | 0 |
| [`Resources/ObjectStream/SerializableAttributeTyped.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableAttributeTyped.h) | 0 |
| [`Resources/ObjectStream/SerializableObject.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableObject.h) | 0 |
| [`Resources/ObjectStream/TypeDeclarations.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/TypeDeclarations.h) | 0 |
| [`Resources/PCK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/PCK.h) | 0 |
| [`Resources/Timer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Timer.h) | 0 |
| [`Resources/Tween.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Tween.h) | 0 |

### Skeleton (1 headers)

Skeleton and animation declarations.

| Header | Export-marked declarations |
|---|---:|
| [`Skeleton/Skeleton.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Skeleton/Skeleton.h) | 0 |

### TriangleSplitter (1 headers)

Triangle splitting utilities.

| Header | Export-marked declarations |
|---|---:|
| [`TriangleSplitter/TriangleSplitter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/TriangleSplitter/TriangleSplitter.h) | 0 |

### Variants (53 headers)

Math, geometry and container value types.

| Header | Export-marked declarations |
|---|---:|
| [`Variants/AABB2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/AABB2.h) | 0 |
| [`Variants/AABB3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/AABB3.h) | 0 |
| [`Variants/Basis.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Basis.h) | 0 |
| [`Variants/Color.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Color.h) | 0 |
| [`Variants/Math/EigenValueSymmetric.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/EigenValueSymmetric.h) | 0 |
| [`Variants/Math/GaussianElimination.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/GaussianElimination.h) | 0 |
| [`Variants/Math/HalfFloat.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/HalfFloat.h) | 0 |
| [`Variants/Math/MathTypes.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/MathTypes.h) | 0 |
| [`Variants/Math/Real.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/Real.h) | 0 |
| [`Variants/Math/Trigonometry.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/Trigonometry.h) | 0 |
| [`Variants/Matrix/DMat44.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DMat44.h) | 0 |
| [`Variants/Matrix/DMat4x3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DMat4x3.h) | 0 |
| [`Variants/Matrix/DynMatrix.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DynMatrix.h) | 0 |
| [`Variants/Matrix/Mat22.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat22.h) | 0 |
| [`Variants/Matrix/Mat2x3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat2x3.h) | 0 |
| [`Variants/Matrix/Mat2x4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat2x4.h) | 0 |
| [`Variants/Matrix/Mat33.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat33.h) | 0 |
| [`Variants/Matrix/Mat3x2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat3x2.h) | 0 |
| [`Variants/Matrix/Mat3x4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat3x4.h) | 0 |
| [`Variants/Matrix/Mat44.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat44.h) | 0 |
| [`Variants/Matrix/Mat4x2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat4x2.h) | 0 |
| [`Variants/Matrix/Mat4x3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat4x3.h) | 0 |
| [`Variants/Matrix/TMatrix.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/TMatrix.h) | 0 |
| [`Variants/OBB2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/OBB2.h) | 0 |
| [`Variants/OBB3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/OBB3.h) | 0 |
| [`Variants/Plane.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Plane.h) | 0 |
| [`Variants/Quat.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Quat.h) | 0 |
| [`Variants/Rect.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Rect.h) | 0 |
| [`Variants/TArray.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TArray.h) | 0 |
| [`Variants/TMap.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TMap.h) | 0 |
| [`Variants/TSet.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TSet.h) | 0 |
| [`Variants/TStaticArray.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TStaticArray.h) | 0 |
| [`Variants/Vector/BVec16.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/BVec16.h) | 0 |
| [`Variants/Vector/DVec2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec2.h) | 0 |
| [`Variants/Vector/DVec3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec3.h) | 0 |
| [`Variants/Vector/DVec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec4.h) | 0 |
| [`Variants/Vector/Double2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Double2.h) | 0 |
| [`Variants/Vector/Double3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Double3.h) | 0 |
| [`Variants/Vector/Double4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Double4.h) | 0 |
| [`Variants/Vector/Float2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Float2.h) | 0 |
| [`Variants/Vector/Float3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Float3.h) | 0 |
| [`Variants/Vector/Float4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Float4.h) | 0 |
| [`Variants/Vector/Int2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Int2.h) | 0 |
| [`Variants/Vector/Int3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Int3.h) | 0 |
| [`Variants/Vector/Int4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Int4.h) | 0 |
| [`Variants/Vector/TVec.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/TVec.h) | 0 |
| [`Variants/Vector/UVec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/UVec4.h) | 0 |
| [`Variants/Vector/Vec2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec2.h) | 1 |
| [`Variants/Vector/Vec3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec3.h) | 1 |
| [`Variants/Vector/Vec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec4.h) | 0 |
| [`Variants/Vector/iVec2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec2.h) | 1 |
| [`Variants/Vector/iVec3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec3.h) | 1 |
| [`Variants/Vector/iVec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec4.h) | 1 |

### Top-level subsystem headers (11 headers)

| Header | Export-marked declarations |
|---|---:|
| [`Moss.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss.h)` | 0 |
| [`Moss_Audio.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Audio.h)` | 113 |
| [`Moss_GPU.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_GPU.h)` | 241 |
| [`Moss_GUI.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_GUI.h)` | 73 |
| [`Moss_Navigation.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Navigation.h)` | 0 |
| [`Moss_Network.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Network.h)` | 146 |
| [`Moss_Physics.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Physics.h)` | 39 |
| [`Moss_Platform.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Platform.h)` | 181 |
| [`Moss_Renderer.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Renderer.h)` | 195 |
| [`Moss_XR.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_XR.h)` | 41 |
| [`Moss_stdinc.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_stdinc.h)` | 34 |

## Inline implementation header inventory (19 files)

These files provide inline/template definitions or companion implementation details. They are included here because the public API surface cannot be understood from declarations alone when these files contain required definitions.

| Inline header |
|---|
| [`Core/FixedSizeFreeList.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FixedSizeFreeList.inl) |
| [`Core/JobSystem.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystem.inl) |
| [`Core/LockFreeHashMap.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/LockFreeHashMap.inl) |
| [`Core/Profiler.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Profiler.inl) |
| [`Physics/Body/Body.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Body/Body.inl) |
| [`Variants/Matrix/DMat44.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DMat44.inl) |
| [`Variants/Matrix/Mat44.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat44.inl) |
| [`Variants/Quat.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Quat.inl) |
| [`Variants/Vector/BVec16.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/BVec16.inl) |
| [`Variants/Vector/DVec2.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec2.inl) |
| [`Variants/Vector/DVec3.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec3.inl) |
| [`Variants/Vector/DVec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec4.inl) |
| [`Variants/Vector/UVec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/UVec4.inl) |
| [`Variants/Vector/Vec2.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec2.inl) |
| [`Variants/Vector/Vec3.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec3.inl) |
| [`Variants/Vector/Vec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec4.inl) |
| [`Variants/Vector/iVec2.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec2.inl) |
| [`Variants/Vector/iVec3.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec3.inl) |
| [`Variants/Vector/iVec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec4.inl) |

## 9. Native integration details to preserve

This is a language-level API specification, not a mechanical ABI mirror. When implementing the projection, keep these source-level distinctions intact:

- Convert pointer-plus-count inputs into a single slice/span/list/array/table/sequence; do not expose a redundant count parameter when the collection already carries its length.
- Convert output pointer pairs (for example, mouse coordinates) into a point/tuple/value object. Do not expose C-style `int*` output arguments.
- Resource factories return owned language objects and translate native allocation/backend failure into the language's normal error model. Destroy/terminate functions become deterministic resource disposal (`Dispose`, `Drop`, `close`, or context-manager release), not ordinary public `destroy_*` calls.
- GUI calls should be scoped to a `GuiContext` instance in every managed binding. They must not silently depend on mutable global “current context” state when the public API can retain an explicit context.
- Vulkan handles, callback pointers, allocator callbacks and backend-specific opaque handles stay behind advanced interop APIs unless the user explicitly opts into native interop. They must not be confused with Moss-owned managed resources.
- Where the native declaration lacks information required by the intended safe API (for example a pointer/count pair, output values, or a resource owner), resolve the native declaration or binding-side contract before implementing that projection; do not guess buffer length or object lifetime.

## 10. Build and distribution contract

- Ship matching native binaries and binding artifacts for each supported OS/architecture combination.
- Keep ABI declarations internal; public API users should not need to know calling conventions, struct packing, pointer widths or native library search paths.
- Make backend and optional dependency feature flags visible through capability queries or build metadata.
- Test a clean build, import/load, initialization/shutdown, object lifetime, callback detach, error translation and resource ownership in CI for every supported platform.
- Keep the generated language API index in sync with the native CMoss export catalogue whenever public native declarations change.

## 11. Compatibility and versioning

Treat the binding's public API as a separately versioned language library. A native API addition does not become a public binding API until the type mapping, ownership, failure semantics and example are specified. Avoid leaking raw `Moss_*` function names into application code. Deprecate public methods using the target language's normal annotation mechanism and provide a migration note in the API Reference itself when a breaking change is unavoidable.

---

**Canonical authority:** Moss C++/CMoss headers and the CMoss API Reference supplied for this documentation set. The native symbol column preserves traceability; the public API column is the intended language-specific design.
