#include "UI/SCodeArenaHUD.h"

#include "CodeArenaContent.h"
#include "CodeArenaGameMode.h"

#include "Framework/Text/TextLayout.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CodeArena"

namespace CodeArenaHUDLocal
{
	const int32 MaxConsoleLines = 200;
	const FLinearColor PanelColor(0.02f, 0.025f, 0.035f, 0.93f);
	const FLinearColor CardColor(0.f, 0.f, 0.f, 0.7f);
	const FLinearColor Gold(1.f, 0.8f, 0.2f);
	const FLinearColor PlayerColor(0.25f, 0.55f, 1.f);
	const FLinearColor EnemyColor(1.f, 0.3f, 0.25f);
	const FLinearColor ButtonNeutral(0.35f, 0.38f, 0.45f);
	const FLinearColor ButtonGo(0.15f, 0.6f, 0.25f);
	const FLinearColor ButtonStop(0.7f, 0.18f, 0.15f);
	const FLinearColor ButtonShop(0.65f, 0.5f, 0.1f);
}

void SCodeArenaHUD::Construct(const FArguments& InArgs)
{
	using namespace CodeArenaHUDLocal;
	GameMode = InArgs._GameMode;

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 22);
	const FSlateFontInfo HeaderFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	const FSlateFontInfo CodeFont = FCoreStyle::GetDefaultFontStyle("Mono", 12);
	const FSlateFontInfo HelpFont = FCoreStyle::GetDefaultFontStyle("Mono", 10);
	const FSlateFontInfo BannerFont = FCoreStyle::GetDefaultFontStyle("Bold", 44);

	const FString InitialCode = GetGameMode() ? GetGameMode()->GetCode() : FString();

	ChildSlot
	[
		SNew(SOverlay)

		// ------------------------------------------------------------------
		// Main layout: editor column on the left, fight status on the right
		// ------------------------------------------------------------------
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.44f)
			[
				SNew(SBorder)
				.BorderImage(&PanelBrush)
				.BorderBackgroundColor(PanelColor)
				.Padding(14.f)
				[
					SNew(SVerticalBox)

					// Title + gold
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Title", "CODE ARENA"))
							.Font(TitleFont)
							.ColorAndOpacity(FLinearColor::White)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(HeaderFont)
							.ColorAndOpacity(Gold)
							.Text_Lambda([this]()
							{
								const ACodeArenaGameMode* GM = GetGameMode();
								return FText::Format(LOCTEXT("Gold", "{0} ouro"), FText::AsNumber(GM ? GM->GetGold() : 0));
							})
						]
					]

					// Level selector
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 10.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							MakeButton(FText::FromString(TEXT("<")), [this]()
							{
								if (ACodeArenaGameMode* GM = GetGameMode())
								{
									GM->SelectLevel(GM->GetSelectedLevel() - 1);
								}
							}, ButtonNeutral, TAttribute<bool>::CreateLambda([this]()
							{
								const ACodeArenaGameMode* GM = GetGameMode();
								return GM && GM->GetSelectedLevel() > 0;
							}))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						.Padding(10.f, 0.f)
						[
							SNew(STextBlock)
							.Font(HeaderFont)
							.ColorAndOpacity(FLinearColor::White)
							.Text_Lambda([this]()
							{
								const ACodeArenaGameMode* GM = GetGameMode();
								if (!GM)
								{
									return FText::GetEmpty();
								}
								const int32 Level = GM->GetSelectedLevel();
								return FText::Format(LOCTEXT("LevelTitle", "Nível {0}/{1}: {2}{3}"),
									FText::AsNumber(Level + 1), FText::AsNumber(GM->GetNumLevels()),
									CodeArenaContent::GetLevelText(Level).Title,
									GM->IsLevelBeaten(Level) ? LOCTEXT("Beaten", "  (vencido)") : FText::GetEmpty());
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							MakeButton(FText::FromString(TEXT(">")), [this]()
							{
								if (ACodeArenaGameMode* GM = GetGameMode())
								{
									GM->SelectLevel(GM->GetSelectedLevel() + 1);
								}
							}, ButtonNeutral, TAttribute<bool>::CreateLambda([this]()
							{
								const ACodeArenaGameMode* GM = GetGameMode();
								return GM && GM->IsLevelAvailable(GM->GetSelectedLevel() + 1);
							}))
						]
					]

					// Level description
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 8.f)
					[
						SNew(STextBlock)
						.Font(BodyFont)
						.AutoWrapText(true)
						.ColorAndOpacity(FLinearColor(0.75f, 0.8f, 0.9f))
						.Text_Lambda([this]()
						{
							const ACodeArenaGameMode* GM = GetGameMode();
							return GM ? CodeArenaContent::GetLevelText(GM->GetSelectedLevel()).Description : FText::GetEmpty();
						})
					]

					// Code editor
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						SAssignNew(Editor, SMultiLineEditableTextBox)
						.Font(CodeFont)
						.Text(FText::FromString(InitialCode))
						.AutoWrapText(false)
						.BackgroundColor(FLinearColor(0.05f, 0.06f, 0.08f))
						.ForegroundColor(FLinearColor(0.85f, 0.95f, 0.85f))
						.OnTextChanged_Lambda([this](const FText& NewText)
						{
							if (ACodeArenaGameMode* GM = GetGameMode())
							{
								GM->SetCode(NewText.ToString());
							}
						})
						.OnCursorMoved_Lambda([this](const FTextLocation& Location)
						{
							CursorLine = Location.GetLineIndex();
							CursorColumn = Location.GetOffset();
						})
						.OnKeyDownHandler_Lambda([this](const FGeometry& Geometry, const FKeyEvent& KeyEvent)
						{
							return HandleEditorKeyDown(Geometry, KeyEvent);
						})
					]

					// Editor status line
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 4.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(SmallFont)
						.ColorAndOpacity(FLinearColor(0.6f, 0.65f, 0.7f))
						.Text_Lambda([this]()
						{
							const ACodeArenaGameMode* GM = GetGameMode();
							FString Status = FString::Printf(TEXT("Ln %d, Col %d   |   %d operações por turno"), CursorLine + 1, CursorColumn + 1, GM ? GM->GetOpsPerTick() : 0);
							if (GM && GM->IsMatchRunning() && GM->GetPlayerLine() > 0)
							{
								Status += FString::Printf(TEXT("   |   a executar a linha %d"), GM->GetPlayerLine());
							}
							Status += TEXT("   |   F5 executa");
							return FText::FromString(Status);
						})
					]

					// Buttons
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 8.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 6.f, 0.f)
						[
							MakeButton(TAttribute<FText>::CreateLambda([this]()
							{
								const ACodeArenaGameMode* GM = GetGameMode();
								return GM && GM->IsMatchRunning() ? LOCTEXT("Stop", "Parar") : LOCTEXT("Run", "Executar");
							}),
							[this]() { ToggleRun(); },
							TAttribute<FSlateColor>::CreateLambda([this]()
							{
								const ACodeArenaGameMode* GM = GetGameMode();
								return FSlateColor(GM && GM->IsMatchRunning() ? ButtonStop : ButtonGo);
							}))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 6.f, 0.f)
						[
							MakeButton(TAttribute<FText>::CreateLambda([this]()
							{
								const ACodeArenaGameMode* GM = GetGameMode();
								return GM ? GM->GetSpeedText() : FText::GetEmpty();
							}),
							[this]()
							{
								if (ACodeArenaGameMode* GM = GetGameMode())
								{
									GM->CycleSpeed();
								}
							}, ButtonNeutral)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 6.f, 0.f)
						[
							MakeButton(LOCTEXT("Shop", "Loja"), [this]() { TogglePanel(EPanel::Shop); }, ButtonShop)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 6.f, 0.f)
						[
							MakeButton(LOCTEXT("Help", "Ajuda"), [this]() { TogglePanel(EPanel::Help); }, ButtonNeutral)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							MakeButton(LOCTEXT("ResetCode", "Código inicial"), [this]()
							{
								if (ACodeArenaGameMode* GM = GetGameMode())
								{
									GM->ResetCodeToStarter();
								}
							}, ButtonNeutral)
						]
					]

					// Console
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.HeightOverride(190.f)
						[
							SNew(SBorder)
							.BorderImage(&PanelBrush)
							.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.65f))
							.Padding(6.f)
							[
								SAssignNew(ConsoleScroll, SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(ConsoleList, SVerticalBox)
								]
							]
						]
					]
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.56f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(16.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.Padding(0.f, 0.f, 8.f, 0.f)
					[
						MakeFighterCard(0)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.Padding(8.f, 0.f, 0.f, 0.f)
					[
						MakeFighterCard(1)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Font(HeaderFont)
					.ColorAndOpacity(FLinearColor::White)
					.ShadowOffset(FVector2D(1.f, 1.f))
					.Text_Lambda([this]()
					{
						const ACodeArenaGameMode* GM = GetGameMode();
						if (!GM || GM->GetMatchTick() == 0)
						{
							return FText::GetEmpty();
						}
						return FText::Format(LOCTEXT("Tick", "Turno {0} / {1}"), FText::AsNumber(GM->GetMatchTick()), FText::AsNumber(GM->GetMaxTicks()));
					})
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(BannerFont)
					.ShadowOffset(FVector2D(3.f, 3.f))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
					.Text_Lambda([this]()
					{
						const ACodeArenaGameMode* GM = GetGameMode();
						return GM ? GM->GetBannerText() : FText::GetEmpty();
					})
					.ColorAndOpacity_Lambda([this]()
					{
						const ACodeArenaGameMode* GM = GetGameMode();
						return FSlateColor(GM ? GM->GetBannerColor() : FLinearColor::White);
					})
				]
			]
		]

		// ------------------------------------------------------------------
		// Shop / help panel (over the arena)
		// ------------------------------------------------------------------
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0.f, 24.f, 24.f, 24.f))
		[
			SNew(SBox)
			.WidthOverride(760.f)
			.Visibility_Lambda([this]() { return ActivePanel == EPanel::None ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(SBorder)
				.BorderImage(&PanelBrush)
				.BorderBackgroundColor(FLinearColor(0.03f, 0.035f, 0.05f, 0.97f))
				.Padding(14.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 10.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(TitleFont)
							.ColorAndOpacity(FLinearColor::White)
							.Text_Lambda([this]()
							{
								return ActivePanel == EPanel::Shop ? LOCTEXT("ShopTitle", "Loja") : LOCTEXT("HelpTitle", "Ajuda");
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.f, 0.f, 12.f, 0.f)
						[
							SNew(STextBlock)
							.Font(HeaderFont)
							.ColorAndOpacity(Gold)
							.Visibility_Lambda([this]() { return ActivePanel == EPanel::Shop ? EVisibility::Visible : EVisibility::Collapsed; })
							.Text_Lambda([this]()
							{
								const ACodeArenaGameMode* GM = GetGameMode();
								return FText::Format(LOCTEXT("GoldShop", "{0} ouro"), FText::AsNumber(GM ? GM->GetGold() : 0));
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							MakeButton(LOCTEXT("Close", "Fechar"), [this]() { TogglePanel(EPanel::None); }, ButtonNeutral)
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.f)
					[
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda([this]() { return ActivePanel == EPanel::Help ? 1 : 0; })
						+ SWidgetSwitcher::Slot()
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(ShopList, SVerticalBox)
							]
						]
						+ SWidgetSwitcher::Slot()
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							[
								SNew(STextBlock)
								.Font(HelpFont)
								.ColorAndOpacity(FLinearColor(0.85f, 0.9f, 0.95f))
								.Text(CodeArenaContent::GetHelpText())
							]
							+ SScrollBox::Slot()
							.Padding(0.f, 16.f, 0.f, 0.f)
							.HAlign(HAlign_Left)
							[
								MakeButton(TAttribute<FText>::CreateLambda([this]()
								{
									return bConfirmReset ? LOCTEXT("ConfirmReset", "Tens a certeza? Clica outra vez para apagar tudo") : LOCTEXT("ResetAll", "Apagar progresso");
								}),
								[this]()
								{
									if (!bConfirmReset)
									{
										bConfirmReset = true;
										return;
									}
									bConfirmReset = false;
									if (ACodeArenaGameMode* GM = GetGameMode())
									{
										GM->ResetProgress();
									}
								}, ButtonStop)
							]
						]
					]
				]
			]
		]
	];

	RebuildShop();
}

TSharedRef<SWidget> SCodeArenaHUD::MakeButton(const TAttribute<FText>& Label, TFunction<void()> OnClick,
	const TAttribute<FSlateColor>& Color, const TAttribute<bool>& Enabled)
{
	return SNew(SButton)
		.ButtonColorAndOpacity(Color)
		.IsEnabled(Enabled)
		.ContentPadding(FMargin(12.f, 5.f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.OnClicked_Lambda([OnClick]()
		{
			OnClick();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			.ColorAndOpacity(FLinearColor::White)
		];
}

TSharedRef<SWidget> SCodeArenaHUD::MakeFighterCard(int32 Index)
{
	using namespace CodeArenaHUDLocal;
	const FLinearColor TeamColor = Index == 0 ? PlayerColor : EnemyColor;

	return SNew(SBorder)
		.BorderImage(&PanelBrush)
		.BorderBackgroundColor(CardColor)
		.Padding(10.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					.ColorAndOpacity(TeamColor)
					.Text_Lambda([this, Index]()
					{
						FCodeArenaFighterInfo Info;
						const ACodeArenaGameMode* GM = GetGameMode();
						return GM && GM->GetFighterInfo(Index, Info) ? FText::FromString(Info.Name) : FText::GetEmpty();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
					.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
					.Text_Lambda([this, Index]()
					{
						FCodeArenaFighterInfo Info;
						const ACodeArenaGameMode* GM = GetGameMode();
						return GM && GM->GetFighterInfo(Index, Info) ? FText::FromString(Info.Action) : FText::GetEmpty();
					})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 6.f, 0.f, 3.f)
			[
				SNew(SBox)
				.HeightOverride(20.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SProgressBar)
						.FillColorAndOpacity(TeamColor)
						.Percent_Lambda([this, Index]() -> TOptional<float>
						{
							FCodeArenaFighterInfo Info;
							const ACodeArenaGameMode* GM = GetGameMode();
							if (GM && GM->GetFighterInfo(Index, Info) && Info.MaxHp > 0)
							{
								return static_cast<float>(Info.Hp) / static_cast<float>(Info.MaxHp);
							}
							return 0.f;
						})
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.ColorAndOpacity(FLinearColor::White)
						.ShadowOffset(FVector2D(1.f, 1.f))
						.Text_Lambda([this, Index]()
						{
							FCodeArenaFighterInfo Info;
							const ACodeArenaGameMode* GM = GetGameMode();
							if (GM && GM->GetFighterInfo(Index, Info))
							{
								return FText::FromString(FString::Printf(TEXT("Vida %d / %d"), Info.Hp, Info.MaxHp));
							}
							return FText::GetEmpty();
						})
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(14.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SProgressBar)
						.FillColorAndOpacity(FLinearColor(1.f, 0.85f, 0.15f))
						.Percent_Lambda([this, Index]() -> TOptional<float>
						{
							FCodeArenaFighterInfo Info;
							const ACodeArenaGameMode* GM = GetGameMode();
							if (GM && GM->GetFighterInfo(Index, Info) && Info.MaxEnergy > 0)
							{
								return static_cast<float>(Info.Energy) / static_cast<float>(Info.MaxEnergy);
							}
							return 0.f;
						})
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
						.ColorAndOpacity(FLinearColor::Black)
						.Text_Lambda([this, Index]()
						{
							FCodeArenaFighterInfo Info;
							const ACodeArenaGameMode* GM = GetGameMode();
							if (GM && GM->GetFighterInfo(Index, Info))
							{
								return FText::FromString(FString::Printf(TEXT("Energia %d"), Info.Energy));
							}
							return FText::GetEmpty();
						})
					]
				]
			]
		];
}

void SCodeArenaHUD::RebuildShop()
{
	using namespace CodeArenaHUDLocal;
	if (!ShopList.IsValid())
	{
		return;
	}
	ShopList->ClearChildren();

	for (const CodeArenaContent::FUnlockDef& Def : CodeArenaContent::GetUnlocks())
	{
		const FString Id = Def.Id;
		const FString Requires = Def.Requires;
		const int32 Cost = Def.Cost;

		ShopList->AddSlot()
		.AutoHeight()
		.Padding(0.f, 3.f)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.BorderBackgroundColor(FLinearColor(0.08f, 0.09f, 0.12f, 1.f))
			.Padding(10.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FLinearColor::White)
						.Text(Def.Name)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						.ColorAndOpacity(FLinearColor(0.75f, 0.8f, 0.85f))
						.AutoWrapText(true)
						.Text(Def.Description)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.f, 0.f, 0.f, 0.f)
				[
					SNew(SBox)
					.MinDesiredWidth(150.f)
					[
						MakeButton(TAttribute<FText>::CreateLambda([this, Id, Requires, Cost]()
						{
							const ACodeArenaGameMode* GM = GetGameMode();
							if (GM && GM->HasUnlock(Id))
							{
								return LOCTEXT("Owned", "Comprado");
							}
							if (GM && !Requires.IsEmpty() && !GM->HasUnlock(Requires))
							{
								for (const CodeArenaContent::FUnlockDef& Other : CodeArenaContent::GetUnlocks())
								{
									if (Other.Id == Requires)
									{
										return FText::Format(LOCTEXT("Requires", "Requer {0}"), Other.Name);
									}
								}
							}
							return FText::Format(LOCTEXT("Cost", "{0} ouro"), FText::AsNumber(Cost));
						}),
						[this, Id]()
						{
							if (ACodeArenaGameMode* GM = GetGameMode())
							{
								GM->Buy(Id);
							}
						},
						TAttribute<FSlateColor>::CreateLambda([this, Id]()
						{
							const ACodeArenaGameMode* GM = GetGameMode();
							return FSlateColor(GM && GM->HasUnlock(Id) ? ButtonGo : ButtonShop);
						}),
						TAttribute<bool>::CreateLambda([this, Id]()
						{
							const ACodeArenaGameMode* GM = GetGameMode();
							return GM && GM->CanBuy(Id);
						}))
					]
				]
			]
		];
	}
}

void SCodeArenaHUD::TogglePanel(EPanel Panel)
{
	ActivePanel = ActivePanel == Panel ? EPanel::None : Panel;
	bConfirmReset = false;
}

void SCodeArenaHUD::ToggleRun()
{
	ACodeArenaGameMode* GM = GetGameMode();
	if (!GM)
	{
		return;
	}
	if (GM->IsMatchRunning())
	{
		GM->StopMatch();
	}
	else
	{
		GM->SetCode(Editor->GetText().ToString());
		ActivePanel = EPanel::None;
		GM->StartMatch();
	}
}

FReply SCodeArenaHUD::HandleEditorKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();

	if (Key == EKeys::F5 || (Key == EKeys::Enter && KeyEvent.IsControlDown()))
	{
		ToggleRun();
		return FReply::Handled();
	}

	// Tab inserts 4 spaces (Python-style indentation).
	if (Key == EKeys::Tab && !KeyEvent.IsControlDown() && !KeyEvent.IsAltDown())
	{
		Editor->InsertTextAtCursor(FText::FromString(TEXT("    ")));
		return FReply::Handled();
	}

	// Enter keeps the indentation of the current line, plus one level after a ':'.
	if (Key == EKeys::Enter && !KeyEvent.IsShiftDown() && !KeyEvent.IsAltDown())
	{
		TArray<FString> Lines;
		Editor->GetText().ToString().ParseIntoArrayLines(Lines, false);
		FString Indent;
		if (Lines.IsValidIndex(CursorLine))
		{
			const FString& Line = Lines[CursorLine];
			int32 Spaces = 0;
			while (Spaces < Line.Len() && Line[Spaces] == TEXT(' '))
			{
				++Spaces;
			}
			Indent = FString::ChrN(Spaces, TEXT(' '));
			if (Line.Left(CursorColumn).TrimEnd().EndsWith(TEXT(":")))
			{
				Indent += TEXT("    ");
			}
		}
		Editor->InsertTextAtCursor(FText::FromString(TEXT("\n") + Indent));
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SCodeArenaHUD::AddConsoleLine(const FString& Text, const FLinearColor& Color)
{
	if (!ConsoleList.IsValid())
	{
		return;
	}
	ConsoleList->AddSlot()
	.AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(Text))
		.Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
		.ColorAndOpacity(Color)
		.AutoWrapText(true)
	];
	++NumConsoleLines;
	if (NumConsoleLines > CodeArenaHUDLocal::MaxConsoleLines && ConsoleList->GetChildren()->Num() > 0)
	{
		ConsoleList->RemoveSlot(ConsoleList->GetChildren()->GetChildAt(0));
		--NumConsoleLines;
	}
	ConsoleScroll->ScrollToEnd();
}

void SCodeArenaHUD::ReloadCode()
{
	if (const ACodeArenaGameMode* GM = GetGameMode())
	{
		Editor->SetText(FText::FromString(GM->GetCode()));
	}
}

TSharedPtr<SWidget> SCodeArenaHUD::GetEditorWidget() const
{
	return Editor;
}

#undef LOCTEXT_NAMESPACE
