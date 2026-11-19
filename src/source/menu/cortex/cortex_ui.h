

#pragma once
#include "imgui.h"
#include <functional>

namespace cx {

enum Icon : int {
    Icon_None      = -1,
    Icon_Crosshair = 0,
    Icon_Shield    = 1,
    Icon_Rotate    = 2,
    Icon_Eye       = 3,
    Icon_Globe     = 4,
    Icon_Grid      = 5,
    Icon_User      = 6,
    Icon_Keyboard  = 7,
    Icon_List      = 8,
    Icon_Pulse     = 9,
    Icon_Folder    = 10,
    Icon_Gear      = 11,
};

void SetBrand(const char* name);                                       
void AddSection(const char* name);                                     
void AddPage(const char* label, int icon, std::function<void()> render); 
void RegisterPages();                                                  

bool BeginColumns(int count = 2);   
void NextColumn();
void EndColumns();

bool BeginCard(const char* id, const char* title, const char* subtitle = nullptr, bool* enabled = nullptr, bool gear = false);
void EndCard();

bool RowToggle(const char* label, bool* v);
bool RowCheck (const char* label, bool* v);
bool RowSlider(const char* label, float* v, float min, float max, const char* fmt = "%.1f");
bool RowRange (const char* label, float* lo, float* hi, float min, float max, const char* fmt = "%.1f - %.1f");
bool RowCombo (const char* label, int* idx, const char* const* items, int count);
bool RowInput (const char* label, char* buf, int buf_size, const char* hint);
bool RowColor (const char* label, float col[3]);

bool Button(const char* label, ImVec2 size = ImVec2(0, 0), bool primary = false); 
bool Toggle(const char* id, bool* v);                                              
bool SearchBar(const char* id, char* buf, int buf_size, const char* hint = "Search...");
void TextDim(const char* text);                                                    

void Toast(const char* text);   

void SetFonts(ImFont* regular, ImFont* small_font, ImFont* bold);
void ApplyTheme();
void RenderMenu(bool* open);
void DrawToasts();
void DrawWatermark(bool visible);

inline bool g_explorerOpen = true;

} 
