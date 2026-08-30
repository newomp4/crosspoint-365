#pragma once

// Binds the UI font slots (UI_10, UI_12, SMALL, UI_TITLE) to the typeface in
// SETTINGS.uiFont. Called once at boot and again when the setting changes, so
// the next repaint of any screen is already in the new face.
void applyUiFont();
