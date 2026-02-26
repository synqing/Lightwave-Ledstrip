export async function initialiseComposerWasm() {
  // Lazy-load generated Emscripten module if present.
  // The dashboard falls back to JS simulation when artefacts are absent.
  const modulePath = "./composer_engine.js";

  try {
    const mod = await import(modulePath);
    if (typeof mod.default !== "function") {
      return null;
    }

    const instance = await mod.default({
      locateFile: (file) => `./wasm/${file}`,
    });

    return {
      instance,
      hasNativeEngine: true,
    };
  } catch {
    return {
      instance: null,
      hasNativeEngine: false,
    };
  }
}
