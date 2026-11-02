# JavaMoss Cheatsheet

> Quick reference for the idiomatic Java public API. The companion [API Reference](API_Reference.md) defines the complete feature map and all exported native-to-language projections. The public calls below are the target design, not a claim that the current binding has implemented every member yet.

## Language design at a glance

The public surface is a managed Java API. Use packages, PascalCase classes, lowerCamelCase methods, immutable option/descriptor objects, `Optional<T>` only for naturally absent values, and `AutoCloseable` for owned native resources.

## Subsystem map

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

## 1. Import Moss

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

## 2. Create a window and pump events

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

## 3. Create and upload a GPU buffer

```java
try (var device = GpuDevice.create(new GpuDeviceDesc());
     var buffer = device.createBuffer(
         new GpuBufferDesc(vertices.length, GpuBufferUsage.VERTEX))) {
    buffer.upload(vertices);
}
```

## 4. Render a frame

```java
try (var frame = renderer.beginFrame()) {
    frame.setCamera(camera);
    frame.drawSprite(sprite);
    frame.drawText("Hello Moss", position, textStyle);
    frame.present();
}
```

## 5. Play audio

```java
Audio.initialize();
try (AudioSource source = Audio.loadWav(Path.of("assets/theme.wav"))) {
    source.play();
    Audio.update(deltaTime);
} finally {
    Audio.shutdown();
}
```

## 6. Build a GUI

```java
try (GuiContext ui = GUI.createContext()) {
    ui.window("Inspector", panel -> {
        panel.text("Player");
        boolean enabled = panel.checkbox("Enabled", currentValue);
    });
}
```

## 7. Simulate physics

```java
try (PhysicsSystem physics = PhysicsSystem.create(settings)) {
    try (Body body = physics.bodies().create(
            BodyOptions.dynamic(Shape.box(size)))) {
        physics.step(deltaTime);
        List<Contact> contacts = physics.query().contacts(body);
    }
}
```

## 8. Networking

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

## 9. XR session

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

## 10. Common type families

| Category | Public types |
|---|---|
| Vectors | `Vec2`, `Vec3`, `Vec4`, float/double/integer variants |
| Matrices and transforms | `Mat22`, `Mat33`, `Mat44`, `DMat44`, `Quat`, `Basis` |
| Geometry | `AABB2`, `AABB3`, `OBB2`, `OBB3`, `Plane`, `Rect`, `Triangle` |
| Colour | `Color` with explicit component range and storage precision |
| Containers | language-native lists/maps/sets or typed Moss containers when allocator semantics matter |
| Results and references | target-language result/error types and explicit owned/borrowed resource wrappers |
| Jobs and synchronization | job system, thread pool, barrier, mutex, semaphore and scoped locks |

## 11. Resource-management rules

Owned handles implement `AutoCloseable`; use try-with-resources. Borrowed views are non-owning and should not implement `close()`. JNI global references for callbacks must be released on unregistration. Never call Java callbacks while holding native locks unless the threading contract explicitly permits it. Byte buffers and typed arrays must document whether data is copied or retained.

## 12. Error-handling rules

Methods that cannot complete throw `MossException` (or a typed subclass). Use `Optional<T>` for a value that may legitimately be absent and primitive `boolean` for status queries. Preserve unsigned native bit patterns; expose helpers for unsigned comparison/conversion rather than silently narrowing values.

## 13. Feature and platform checks

- A declaration in the reference does not mean every backend implements it. Check the selected GPU backend, platform, XR runtime and optional networking/codec features.
- Keep GPU resources associated with the device that created them; release child resources before the device.
- Pair renderer frame begin/end, GUI begin/end, render-pass begin/end and XR frame begin/end operations. Prefer scoped/structured helpers so early returns cannot leave a subsystem unbalanced.
- Poll platform and network events at the cadence required by the subsystem. Do not block a browser/UI thread waiting for asynchronous I/O.
- Use the API Reference's subsystem and header inventory when looking for advanced Navigation, Geometry, Physics, Resources, Skeleton or Variants APIs.

## 14. Where to look next

See [API_Reference.md](API_Reference.md) for the full native symbol crosswalk, target-language signatures, resource models, threading rules, feature coverage and the complete 270-header / 19-inline-header inventory.
