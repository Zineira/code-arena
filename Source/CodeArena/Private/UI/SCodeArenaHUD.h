#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/SCompoundWidget.h"

class ACodeArenaGameMode;
class SMultiLineEditableTextBox;
class SScrollBox;
class SVerticalBox;

/**
 * Whole game UI built in Slate (no Blueprint assets needed):
 * code editor, run controls, console, fighter status, shop and help panels.
 */
class SCodeArenaHUD : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCodeArenaHUD) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ACodeArenaGameMode>, GameMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void AddConsoleLine(const FString& Text, const FLinearColor& Color);
	/** Re-reads the code of the selected level from the game mode. */
	void ReloadCode();
	TSharedPtr<SWidget> GetEditorWidget() const;

private:
	enum class EPanel : uint8
	{
		None,
		Shop,
		Help
	};

	TSharedRef<SWidget> MakeButton(const TAttribute<FText>& Label, TFunction<void()> OnClick,
		const TAttribute<FSlateColor>& Color, const TAttribute<bool>& Enabled = true);
	TSharedRef<SWidget> MakeFighterCard(int32 Index);
	void RebuildShop();
	void TogglePanel(EPanel Panel);
	void ToggleRun();
	FReply HandleEditorKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent);
	ACodeArenaGameMode* GetGameMode() const { return GameMode.Get(); }

	TWeakObjectPtr<ACodeArenaGameMode> GameMode;
	TSharedPtr<SMultiLineEditableTextBox> Editor;
	TSharedPtr<SScrollBox> ConsoleScroll;
	TSharedPtr<SVerticalBox> ConsoleList;
	TSharedPtr<SVerticalBox> ShopList;
	EPanel ActivePanel = EPanel::None;
	int32 CursorLine = 0;
	int32 CursorColumn = 0;
	int32 NumConsoleLines = 0;
	bool bConfirmReset = false;
	FSlateColorBrush PanelBrush{ FLinearColor::White };
};
