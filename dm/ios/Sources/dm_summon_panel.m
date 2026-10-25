// 召唤控制台的界面：游戏画面边上一个可拖动的圆按钮，点开一块面板，分「召唤」「修改」两页。原生 UIKit，不改游戏布局。
// 按钮只调 dm/host/dm_summon.h 的投请求函数，执行都在服务端拍里（dm_summon.c）。挂在 DmMetalView 上（front_ios.m），前端页面（选角等）藏起来。
// UI of the summon console: a draggable round button at the edge of the game view; tapping it opens a panel with two pages, "Summon" and "Cheats". Native UIKit, the game layouts are untouched.
// Buttons only call the request functions in dm/host/dm_summon.h; everything executes in the server tick (dm_summon.c). Attached to DmMetalView (front_ios.m), hidden on front-end pages (character select etc.).
#import <UIKit/UIKit.h>
#include "../../host/dm_summon.h"
#include "dm_i18n.h"

// 面板右下角的版本号：每次交给玩家测试的构建都改这里，截图里一眼看出装的是哪一版 | version shown at the panel's bottom-right: bumped for every build handed out for testing, so a screenshot shows which one is installed
#define DM_SUMMON_UI_VER "summon console 9"

@interface DmSummonPanel : NSObject
@property (nonatomic, strong) UIButton *fab;
@property (nonatomic, strong) UIView *panel;
@property (nonatomic, strong) UISegmentedControl *tabs, *bindSeg;
@property (nonatomic, strong) UIView *pageSummon;
@property (nonatomic, strong) UIScrollView *pageCheat;   // 按钮 + 一列开关，比面板高，可滚动 | buttons + a column of switches, taller than the panel, scrollable
@property (nonatomic, strong) NSMutableArray<UIButton *> *bossButtons;
@property (nonatomic, strong) UILabel *countLabel, *aliveLabel;
@property (nonatomic) int count;   // 召唤数量 1–10（−/+ 两个按钮调；系统 UIStepper 在深色面板上看不见，真机截图） | summon count 1–10 (two −/+ buttons; the system UIStepper is invisible on the dark panel, device screenshot)
@property (nonatomic, strong) NSTimer *timer;
@property (nonatomic) int boss;
@property (nonatomic) BOOL moved;   // 玩家拖过按钮：之后不再自动放回左上角 | the player has dragged the button: no longer snapped back to the top-left
@end

static DmSummonPanel *g_panel;

static UIButton *mk_button(NSString *title, id target, SEL action) {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    [b setTitle:title forState:UIControlStateNormal];
    b.titleLabel.font = [UIFont boldSystemFontOfSize:15];
    b.titleLabel.adjustsFontSizeToFitWidth = YES;
    [b setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
    b.backgroundColor = [UIColor colorWithWhite:0.22 alpha:1];
    b.layer.cornerRadius = 8; b.layer.borderWidth = 1; b.layer.borderColor = [UIColor colorWithRed:0.62 green:0.5 blue:0.3 alpha:1].CGColor;
    [b addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];
    return b;
}
static void style_seg(UISegmentedControl *g) {   // 深色面板上看得清的分段控件：灰底、金色选中、白字、金边 | a segmented control readable on the dark panel: gray base, gold selection, white text, gold border
    g.backgroundColor = [UIColor colorWithWhite:0.22 alpha:1]; g.selectedSegmentTintColor = [UIColor colorWithRed:0.55 green:0.42 blue:0.18 alpha:1];
    NSDictionary *a = @{ NSForegroundColorAttributeName: UIColor.whiteColor, NSFontAttributeName: [UIFont boldSystemFontOfSize:15] };
    [g setTitleTextAttributes:a forState:UIControlStateNormal]; [g setTitleTextAttributes:a forState:UIControlStateSelected];
    g.layer.borderWidth = 1; g.layer.borderColor = [UIColor colorWithRed:0.62 green:0.5 blue:0.3 alpha:1].CGColor;
}
static UILabel *mk_label(NSString *text, CGFloat size) {
    UILabel *l = [UILabel new]; l.text = text; l.textColor = UIColor.whiteColor; l.font = [UIFont systemFontOfSize:size]; l.adjustsFontSizeToFitWidth = YES; return l;
}

@implementation DmSummonPanel
- (instancetype)initInView:(UIView *)host {
    if (!(self = [super init])) return nil;
    _boss = DM_SUM_DIABLO; _bossButtons = [NSMutableArray array];
    _fab = [UIButton buttonWithType:UIButtonTypeCustom];
    _fab.frame = CGRectMake(12, 12, 52, 52); _fab.layer.cornerRadius = 26;
    _fab.backgroundColor = [UIColor colorWithRed:0.45 green:0.08 blue:0.06 alpha:0.85];
    _fab.layer.borderWidth = 2; _fab.layer.borderColor = [UIColor colorWithRed:0.85 green:0.7 blue:0.35 alpha:1].CGColor;
    [_fab setTitle:@(dm_ui("dmSumFab")) forState:UIControlStateNormal]; _fab.titleLabel.font = [UIFont boldSystemFontOfSize:22];
    _fab.accessibilityLabel = @(dm_ui("dmSumConsole"));
    [_fab addTarget:self action:@selector(toggle) forControlEvents:UIControlEventTouchUpInside];
    [_fab addGestureRecognizer:[[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(drag:)]];
    [host addSubview:_fab];

    _panel = [UIView new]; _panel.hidden = YES;
    _panel.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.94]; _panel.layer.cornerRadius = 12;
    _panel.layer.borderWidth = 1; _panel.layer.borderColor = [UIColor colorWithRed:0.62 green:0.5 blue:0.3 alpha:1].CGColor;
    [host addSubview:_panel];
    _tabs = [[UISegmentedControl alloc] initWithItems:@[@(dm_ui("dmSumTabSummon")), @(dm_ui("dmSumTabCheat"))]]; _tabs.selectedSegmentIndex = 0;
    // 系统缺省样式在深色面板上没选中那页的字几乎看不见（真机截图：「修改」页签没人发现）：底色、选中色、字色都显式给
    // With the system default style, the unselected segment's text is almost invisible on the dark panel (device screenshot: nobody found the "Cheats" tab): background, selected tint and text colors are all set explicitly
    style_seg(_tabs);
    UILabel *ver = mk_label(@DM_SUMMON_UI_VER, 11); ver.tag = 905; ver.textColor = [UIColor colorWithWhite:0.6 alpha:1]; ver.textAlignment = NSTextAlignmentRight; [_panel addSubview:ver];
    [_tabs addTarget:self action:@selector(tabChanged) forControlEvents:UIControlEventValueChanged];
    [_panel addSubview:_tabs];
    UIButton *close = mk_button(@(dm_ui("dmSumClose")), self, @selector(toggle)); close.tag = 900; [_panel addSubview:close];

    _pageSummon = [UIView new]; [_panel addSubview:_pageSummon];
    for (int i = 0; i < DM_SUM_NBOSS; i++) { UIButton *b = mk_button(@(dm_ui(dm_summon_key(i))), self, @selector(pickBoss:)); b.tag = i; [_bossButtons addObject:b]; [_pageSummon addSubview:b]; }
    _countLabel = mk_label(@"", 16); [_pageSummon addSubview:_countLabel];
    _count = 1;
    UIButton *minus = mk_button(@"−", self, @selector(countDown)); minus.tag = 903; [_pageSummon addSubview:minus];
    UIButton *plus = mk_button(@"+", self, @selector(countUp)); plus.tag = 904; [_pageSummon addSubview:plus]; [self countChanged];
    UIButton *go = mk_button(@(dm_ui("dmSumGo")), self, @selector(summon)); go.tag = 901; go.backgroundColor = [UIColor colorWithRed:0.42 green:0.1 blue:0.07 alpha:1]; [_pageSummon addSubview:go];
    UIButton *clr = mk_button(@(dm_ui("dmSumClear")), self, @selector(clearAll)); clr.tag = 902; [_pageSummon addSubview:clr];
    // 绑定类型（排查随从猝死：同一局里各试一次就能比出来）| bind type (for chasing sudden minion deaths: one try of each in the same run compares them)
    UILabel *bl = mk_label(@(dm_ui("dmSumBindType")), 14); bl.tag = 906; [_pageSummon addSubview:bl];
    _bindSeg = [[UISegmentedControl alloc] initWithItems:@[@(dm_ui("dmSumBindSkel")), @(dm_ui("dmSumBindDemon")), @(dm_ui("dmSumBindValk")), @(dm_ui("dmSumBindNone"))]];
    style_seg(_bindSeg); _bindSeg.selectedSegmentIndex = dm_summon_bind_mode();
    [_bindSeg addTarget:self action:@selector(bindChanged) forControlEvents:UIControlEventValueChanged]; [_pageSummon addSubview:_bindSeg];
    _aliveLabel = mk_label(@"", 14); [_pageSummon addSubview:_aliveLabel];

    _pageCheat = [UIScrollView new]; _pageCheat.hidden = YES; _pageCheat.indicatorStyle = UIScrollViewIndicatorStyleWhite; [_panel addSubview:_pageCheat];
    NSArray *cheats = @[@[@"dmCheatLv1", @(DM_CHEAT_LEVEL_UP), @0], @[@"dmCheatLv99", @(DM_CHEAT_LEVEL_99), @0], @[@"dmCheatGold", @(DM_CHEAT_GOLD_MAX), @0],
                        @[@"dmCheatStat", @(DM_CHEAT_STATPTS), @10], @[@"dmCheatSkill", @(DM_CHEAT_SKILLPTS), @5], @[@"dmCheatRefill", @(DM_CHEAT_REFILL), @0],
                        @[@"dmCheatWp", @(DM_CHEAT_WAYPOINTS), @0], @[@"dmCheatDiff", @(DM_CHEAT_DIFFICULTY), @0]];
    for (NSArray *c in cheats) { UIButton *b = mk_button(@(dm_ui([c[0] UTF8String])), self, @selector(cheat:)); b.tag = [c[1] intValue] * 1000 + [c[2] intValue]; [_pageCheat addSubview:b]; }
    for (int i = 0; i < 2 + DM_NBUFF; i++) {   // 0 无敌、1 无限法力、其后是属性开关（dm_summon.h DM_BUFF_*） | 0 god mode, 1 infinite mana, then the stat toggles (dm_summon.h DM_BUFF_*)
        UILabel *l = mk_label(@(dm_ui(i == 0 ? "dmCheatGod" : i == 1 ? "dmCheatInfMana" : dm_cheat_buff_key(i - 2))), 15); l.tag = 950 + i; [_pageCheat addSubview:l];
        UISwitch *s = [UISwitch new]; s.tag = 960 + i; s.on = i == 0 ? dm_cheat_god() : i == 1 ? dm_cheat_inf_mana() : dm_cheat_buff(i - 2);
        [s addTarget:self action:@selector(toggleSwitch:) forControlEvents:UIControlEventValueChanged]; [_pageCheat addSubview:s];
    }
    [self refreshBoss];
    return self;
}
- (void)layoutIn:(CGRect)b {
    CGFloat w = MIN(420, b.size.width - 32), h = MIN(330, b.size.height - 32), pad = 12;
    _panel.frame = CGRectMake((b.size.width - w) / 2, (b.size.height - h) / 2, w, h);
    _tabs.frame = CGRectMake(pad, pad, w - 2 * pad - 80, 34);
    [_panel viewWithTag:900].frame = CGRectMake(w - pad - 72, pad, 72, 34);
    [_panel viewWithTag:905].frame = CGRectMake(w - pad - 160, h - 18, 160, 14);
    CGRect page = CGRectMake(pad, pad + 44, w - 2 * pad, h - 2 * pad - 44 - 8); _pageSummon.frame = page; _pageCheat.frame = page;   // 底下留出版本号那一行 | leave the version line free at the bottom
    CGFloat pw = page.size.width, bw = (pw - 2 * 8) / 3, bh = 44;
    for (int i = 0; i < DM_SUM_NBOSS; i++) _bossButtons[i].frame = CGRectMake((i % 3) * (bw + 8), (i / 3) * (bh + 8), bw, bh);
    CGFloat y = 2 * (bh + 8) + 4;
    _countLabel.frame = CGRectMake(0, y, 90, 36); [_pageSummon viewWithTag:903].frame = CGRectMake(96, y - 2, 44, 40); [_pageSummon viewWithTag:904].frame = CGRectMake(148, y - 2, 44, 40); _aliveLabel.frame = CGRectMake(pw - 110, y, 110, 36);
    y += 44;
    [_pageSummon viewWithTag:901].frame = CGRectMake(0, y, (pw - 8) / 2, bh); [_pageSummon viewWithTag:902].frame = CGRectMake((pw + 8) / 2, y, (pw - 8) / 2, bh);
    y += bh + 8; [_pageSummon viewWithTag:906].frame = CGRectMake(0, y, 80, 32); _bindSeg.frame = CGRectMake(84, y, pw - 84, 32);
    int k = 0; for (UIView *v in _pageCheat.subviews) if ([v isKindOfClass:UIButton.class]) { v.frame = CGRectMake((k % 3) * (bw + 8), (k / 3) * (bh + 8), bw, bh); k++; }
    CGFloat top = ((k + 2) / 3) * (bh + 8) + 6;   // 按钮占的行数（每行 3 个） | rows taken by the buttons (3 per row)
    for (int i = 0; i < 2 + DM_NBUFF; i++) { CGFloat yy = top + i * 42; [_pageCheat viewWithTag:950 + i].frame = CGRectMake(0, yy, pw - 70, 36); [_pageCheat viewWithTag:960 + i].frame = CGRectMake(pw - 56, yy + 2, 51, 31); }
    _pageCheat.contentSize = CGSizeMake(pw, top + (2 + DM_NBUFF) * 42);
    [self placeFab:b];
}
- (void)toggle {
    _panel.hidden = !_panel.hidden;
    if (!_panel.hidden) { [_panel.superview bringSubviewToFront:_panel]; [self layoutIn:_panel.superview.bounds]; [self tick];
        _timer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(tick) userInfo:nil repeats:YES]; }
    else { [_timer invalidate]; _timer = nil; }
}
- (void)tick { _aliveLabel.text = [NSString stringWithFormat:@(dm_ui("dmSumAlive")), dm_summon_alive()]; }
// 按钮缺省在左上角（让开刘海 / 圆角的安全区再留 8 点）；拖过以后只保证不出界
// The button defaults to the top-left (inside the safe area for the notch / rounded corners, plus 8 points); once dragged it is only kept on screen
- (void)placeFab:(CGRect)b {
    UIEdgeInsets in = _fab.superview.safeAreaInsets; CGRect f = _fab.frame;
    if (!_moved) f.origin = CGPointMake(in.left + 8, in.top + 8);
    f.origin.x = MAX(0, MIN(f.origin.x, b.size.width - f.size.width)); f.origin.y = MAX(0, MIN(f.origin.y, b.size.height - f.size.height)); _fab.frame = f;
}
- (void)drag:(UIPanGestureRecognizer *)g {
    _moved = YES;
    UIView *host = _fab.superview; CGPoint d = [g translationInView:host]; [g setTranslation:CGPointZero inView:host];
    CGRect f = _fab.frame; f.origin.x = MAX(0, MIN(f.origin.x + d.x, host.bounds.size.width - f.size.width)); f.origin.y = MAX(0, MIN(f.origin.y + d.y, host.bounds.size.height - f.size.height)); _fab.frame = f;
}
- (void)bindChanged { dm_summon_set_bind_mode((int)_bindSeg.selectedSegmentIndex); }
- (void)tabChanged { _pageSummon.hidden = _tabs.selectedSegmentIndex != 0; _pageCheat.hidden = _tabs.selectedSegmentIndex != 1; }
- (void)pickBoss:(UIButton *)b { _boss = (int)b.tag; [self refreshBoss]; }
- (void)refreshBoss { for (UIButton *b in _bossButtons) b.backgroundColor = b.tag == _boss ? [UIColor colorWithRed:0.55 green:0.42 blue:0.18 alpha:1] : [UIColor colorWithWhite:0.22 alpha:1]; }
- (void)countChanged { _countLabel.text = [NSString stringWithFormat:@(dm_ui("dmSumCount")), _count]; }
- (void)countDown { if (_count > 1) _count--; [self countChanged]; }
- (void)countUp { if (_count < 10) _count++; [self countChanged]; }
- (void)summon { dm_summon_request(_boss, _count); }
- (void)clearAll { dm_summon_clear(); }
- (void)cheat:(UIButton *)b { dm_cheat_request((int)(b.tag / 1000), (int)(b.tag % 1000)); }
- (void)toggleSwitch:(UISwitch *)s { if (s.tag == 960) dm_cheat_set_god(s.on); else if (s.tag == 961) dm_cheat_set_inf_mana(s.on); else dm_cheat_set_buff((int)s.tag - 962, s.on); }
@end

void dm_summon_panel_attach(UIView *host) { if (!g_panel) g_panel = [[DmSummonPanel alloc] initInView:host]; }
void dm_summon_panel_layout(CGRect bounds) { if (!g_panel) return; [g_panel placeFab:bounds]; if (!g_panel.panel.hidden) [g_panel layoutIn:bounds]; }
void dm_summon_panel_hidden(BOOL hidden) {   // 前端页面（选角等）整套藏起来，面板顺手关掉 | on front-end pages (character select etc.) hide everything and close the panel
    if (!g_panel) return; g_panel.fab.hidden = hidden; if (hidden && !g_panel.panel.hidden) [g_panel toggle];
}
