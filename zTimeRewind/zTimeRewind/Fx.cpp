// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static zCView* overlay = nullptr;
	static zCView* barBack = nullptr;
	static zCView* barFill = nullptr;
	static bool rewinding = false;
	static float fxRewound = 0.0f, fxAvailable = 0.0f, pulse = 0.0f;
	static int loopHandle = 0;
	static zCSoundFX* sfxStart = nullptr;
	static zCSoundFX* sfxLoop = nullptr;
	static zCSoundFX* sfxEnd = nullptr;

	static zCSoundFX* LoadSfx(const char* name) {
		return zsound ? zsound->LoadSoundFXScript(zSTRING(name)) : nullptr;
	}

	void FxInit() {
		overlay = new zCView(0, 0, 8192, 8192);
		overlay->SetAlphaBlendFunc(zTRnd_AlphaBlendFunc::zRND_ALPHA_FUNC_BLEND);
		overlay->InsertBack(zSTRING("WHITE.TGA"));
		overlay->SetColor(zCOLOR(150, 110, 60, 255));   // sepia
		barBack = new zCView(0, 0, 8192, 8192);
		barBack->InsertBack(zSTRING("BAR_BACK.TGA"));
		barFill = new zCView(0, 0, 8192, 8192);
		barFill->InsertBack(zSTRING("BAR_MISC.TGA"));
		sfxStart = LoadSfx("MFX_TRANSFORM_CAST");
		sfxLoop = LoadSfx("MFX_CONTROL_INVEST");
		sfxEnd = LoadSfx("MFX_TELEPORT_CAST");
		ZLog("Fx: dzwieki start %d petla %d koniec %d", sfxStart != nullptr, sfxLoop != nullptr, sfxEnd != nullptr);
	}

	void FxBegin() {
		rewinding = true;
		pulse = 0.0f;
		if (zsound && sfxStart)
			zsound->PlaySound(sfxStart, 0);
		if (zsound && sfxLoop)
			loopHandle = zsound->PlaySound(sfxLoop, 0);
	}

	void FxEnd() {
		if (!rewinding)
			return;
		rewinding = false;
		if (zsound && loopHandle)
			zsound->StopSound(loopHandle);
		loopHandle = 0;
		if (zsound && sfxEnd)
			zsound->PlaySound(sfxEnd, 0);
	}

	void FxUpdate(float rewound, float available) {
		fxRewound = rewound;
		fxAvailable = available;
	}

	static void Blit(zCView* v) {
		screen->InsertItem(v);
		v->Blit();
		screen->RemoveItem(v);
	}

	void FxRender() {
		if (!overlay || !player || !ogame || ogame->singleStep || ogame->pause_screen)
			return;
		bool rune = SpellRuneReady();
		if (!rewinding && !rune)
			return;
		if (rewinding) {
			pulse += ztimer->frameTimeFloat / 1000.0f;
			int alpha = 90 + (int)(25.0f * sinf(pulse * 4.0f));
			overlay->SetTransparency(alpha);
			Blit(overlay);
		}
		// pasek piasku: ile sekund mozna jeszcze cofnac
		float left = rewinding ? fxAvailable - fxRewound : RecorderSeconds();
		float k = trcfg.bufferSeconds > 0.0f ? left / trcfg.bufferSeconds : 0.0f;
		k = k < 0.0f ? 0.0f : k > 1.0f ? 1.0f : k;
		int w = 2400, h = 160, x = 4096 - w / 2, y = 400;
		barBack->SetPos(x, y);
		barBack->SetSize(w, h);
		Blit(barBack);
		int fw = (int)((w - 120) * k);
		if (fw > 0) {
			barFill->SetPos(x + 60, y + 30);
			barFill->SetSize(fw, h - 60);
			Blit(barFill);
		}
		int sx, sy, ssx, ssy;
		screen->GetViewport(sx, sy, ssx, ssy);
		zrenderer->SetViewport(sx, sy, ssx, ssy);
	}
}
