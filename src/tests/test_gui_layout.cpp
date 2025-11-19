
#include "unittest_internal.h"

#if BUILD_CLIENT

#include "gui/guilayout/guilayout.h"
#include "gui/guilayout/guilayout_irrlicht.h"
#include "gui/guiscript.h"
#include "core/utils.h"
#include <chrono>
// Irrlicht includes
#include <irrlicht.h> // createDeviceEx
#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUITabControl.h>
#include <IVideoDriver.h>


using namespace irr;
using namespace guilayout;

#if 0
	#define DBG_TEST(...) printf(__VA_ARGS__)
#else
	#define DBG_TEST(...) do {} while(0)
#endif

// ------------------- Irrlicht env -------------------

static IrrlichtDevice *device;
static GuiScript *script;

struct UnittestEventReceiver : public IEventReceiver {
	~UnittestEventReceiver() {}

	bool OnEvent(const SEvent &event) override
	{
		if (script)
			return script->OnEvent(event);
		return false;
	}
};

struct TestButton : public IGUIElementWrapper {
	TestButton() :
		IGUIElementWrapper(device->getGUIEnvironment()->addButton({}))
	{
		m_element->setText(L"YES");
		setSize({100, 70});
	}

	inline void setSize(s16_x2 size)
	{
		min_size = size;
	}

	void updateText(u16 a, u16 b)
	{
		char buf[20];
		snprintf(buf, sizeof(buf), "[ %d , %d ]", a, b);
		std::wstring wstr;
		utf8_to_wide(wstr, buf);
		m_element->setText(wstr.c_str());
	}
};

// ------------------- GUI builders -------------------

static Element *setup_table_demo()
{
	static Table root;
	root.setSize(5, 4);
	root.col(4)->weight = 40;
	root.row(0)->weight = 0;
	root.row(3)->weight = 30;
	root.margin = {10, 1, 10, 1};

	for (u16 xp = 0; xp < 4; ++xp) {
		for (u16 yp = 0; yp < 4; ++yp) {
			TestButton *b = root.add<TestButton>(xp, yp);
			b->updateText(xp, yp);

			switch (xp) {
				case 1: b->margin = {5,5,5,5}; break; // x center, y center
				case 2: b->margin = {0,15,15,15}; break; // y center, stick to left
				case 3: b->margin = {0,0,15,15}; break; // stick to top left
				default: b->margin = {0,0,0,0}; break; // fill
			}

			switch (yp) {
				case 1: b->setSize({150, 20}); break;
				case 2: b->setSize({200, (s16)(60 + (xp == 2) * 60)}); break;
				case 3: b->setSize({50, 60}); break;
				default: break;
			}

			b->expand = {1,1};
		}
	}
	return &root;
}

static Element *setup_box_demo()
{
	static FlexBox root;
	root.box_axis = FlexBox::SIZE_X;
	root.allow_wrap = true;

	for (int xp = 2; xp < 4; ++xp) {
		FlexBox *vbox = root.add<FlexBox>();
		vbox->box_axis = FlexBox::SIZE_Y;
		vbox->expand = {5,5};
		vbox->margin = {1,1,1,1};
		vbox->allow_wrap = false;

		if (xp == 1)
			vbox->margin = {10, 0, 10, 0};

		for (int yp = 0; yp < 4; ++yp) {
			TestButton *b = vbox->add<TestButton>();
			b->updateText(xp, yp);

			switch (xp) {
				case 1: b->margin = {5,5,5,5}; break;
				case 2: b->margin = {5,5,5,0}; break;
				case 3: b->margin = {5,0,5,0}; break;
				default: b->margin = {0,0,0,0}; break;
			}

			switch (yp) {
				case 1: b->setSize({200, 20}); break;
				case 2: b->setSize({200, (s16)(60 + (xp == 2) * 60)}); break;
				case 3: b->setSize({50, 60}); break;
				default: break;
			}

			b->expand = {1,1};
		}
	}
	{
		// H box with float-bottom elements
		auto hbox = root.add<FlexBox>();
		hbox->allow_wrap = false;
		//hbox->force_wrap = true;

		auto label = hbox->add<TestButton>();
		label->margin = {10,10,5,10};
		label->setSize({100, 30});

		auto input = hbox->add<TestButton>();
		input->margin = {0,1,0,0};
		input->expand = {1,0};
		input->setSize({200, 30});

		auto btn = hbox->add<TestButton>();
		btn->margin = {0,10,10,0};
	}
	return &root;
}

static Element *setup_tabcontrol()
{
	static FlexBox main;
	core::recti norect(3, 2, 300, 300);
	auto size = device->getVideoDriver()->getScreenSize();
	gui::IGUITabControl *tc = device->getGUIEnvironment()->addTabControl(norect);
	IGUIElementWrapper *wrap = main.add<IGUIElementWrapper>(tc);
	wrap->min_size = {200, (s16)(size.Height * 0.6f) };
	wrap->expand = { 1, 1 };
	wrap->margin = { 1, 1, 1, 1 };
	{
		gui::IGUITab *tab = tc->addTab(L"tab1");
		tab->setBackgroundColor(0xFFFFFFFF);
		tab->setDrawBackground(true);
		Table *box = wrap->add<Table>();
		box->setSize(3, 3);
		box->margin = { 1, 1, 1, 1 };
		auto btn = device->getGUIEnvironment()->addEditBox(L"test", norect, true, tab);
		auto a = box->add<IGUIElementWrapper>(2, 1, btn); // left middle
		a->min_size = { 50, 20};
		a->expand = { 1, 1 };
	}

	{
		gui::IGUITab *tab = tc->addTab(L"tab2");
		tab->setBackgroundColor(0xFF66FFFF);
		tab->setDrawBackground(true);
		FlexBox *box = wrap->add<FlexBox>();
		box->margin = { 3, 2, 1, 1 };
		auto btn = device->getGUIEnvironment()->addButton(norect, tab, 422, L"test");
		auto a = box->add<IGUIElementWrapper>(btn);
		a->min_size = { 50, 20 };
		a->expand = { 1, 1 };
	}

	return &main;
}

// -------------- Basic calculation test --------------

namespace {
	class NopElement : public Element {
	public:
		// none
	};
}

static void test_layout_table_calc()
{
	const u16
		WIDTH = 120,
		HEIGHT = 60;

	Table main;
	main.setSize(3, 2);

	// fill the space to extract the positions
	NopElement *e00 = main.add<NopElement>(0, 0);
	NopElement *e11 = main.add<NopElement>(1, 1);
	NopElement *e21 = main.add<NopElement>(2, 1);


	// ----> X axis checks

	main.col(0)->weight = 1;
	main.col(1)->weight = 3;
	main.col(2)->weight = 2;

	const u16 weight_width = WIDTH / (6 /* weight sum */);
	((Element *)&main)->start({0, 0, WIDTH, HEIGHT});

	u16 v = 0;
	CHECK(e00->pos[Element::DIR_LEFT]  == v + 0);
	// 1px overlap for elements
	CHECK(e00->pos[Element::DIR_RIGHT] == v + 1 * weight_width);
	v += 1 * weight_width;
	CHECK(e11->pos[Element::DIR_LEFT]  == v + 0);
	CHECK(e11->pos[Element::DIR_RIGHT] == v + 3 * weight_width);
	v += 3 * weight_width;
	CHECK(e21->pos[Element::DIR_LEFT]  == v + 0);
	CHECK(e21->pos[Element::DIR_RIGHT] == v + 2 * weight_width);

	// v---- Y axis checks

	main.row(0)->weight = 2;
	main.row(1)->weight = 1;

	const u16 weight_height = HEIGHT / (3 /* weight sum */);
	((Element *)&main)->start({0, 0, WIDTH, HEIGHT});

	v = 0;
	CHECK(e00->pos[Element::DIR_UP]   == v + 0);
	CHECK(e00->pos[Element::DIR_DOWN] == v + 2 * weight_height);
	v += 2 * weight_width;
	CHECK(e21->pos[Element::DIR_UP]   == v + 0);
	CHECK(e21->pos[Element::DIR_DOWN] == v + 1 * weight_height);
}

static void test_layout_flexbox_calc()
{
	fprintf(stderr, "TODO: test_layout_flexbox_calc\n");
}

// ---------------- Script integration ----------------

#include "core/blockmanager.h"
#include "core/world.h"
#include "client/clientmedia.h"
core::recti rect_override;

static Element *setup_guiscript(gui::IGUIEnvironment *guienv)
{
	// similar to test_mediamanger.h / test_with_script
	static Element *e; // never freed
	static BlockManager *bmgr; // never freed (needed by GUI callbacks)

	bmgr = new BlockManager();

	ClientMedia media;
	media.indexAssets();

	script = new GuiScript(bmgr, guienv);
	script->hide_global_table = false;
	script->init();
	script->setMediaMgr(&media);
	script->setTestMode("const gui");
	CHECK(script->loadFromAsset("unittest.lua"));

	gui::IGUIElement *ie_parent = nullptr;

	if (1) {
		rect_override = core::recti(
			core::vector2di(100, 50),
			core::dimension2di(500, 300)
		);

		auto tab = guienv->addTab(rect_override, nullptr, 12345);
		tab->setBackgroundColor(0xFF66FFFF);
		tab->setDrawBackground(true);
		tab->setNotClipped(true);
		ie_parent = tab;
	}

	BlockUpdate bu(bmgr);
	CHECK(bu.set(103));

	e = script->openGUI(bu.getId(), ie_parent);
	Table *le_table = (Table *)e; // not French
	script->linkWithGui(&bu);
	{
		// Test coin input box --> `on_input` callback
		auto eww = (IGUIElementWrapper *)le_table->get(1, 1).get();
		CHECK(eww);

		eww->getElement()->setText(L"98");

		SEvent ev;
		ev.EventType = EET_GUI_EVENT;
		ev.GUIEvent.EventType = gui::EGET_EDITBOX_CHANGED;
		ev.GUIEvent.Caller = eww->getElement();
		ev.GUIEvent.Element = nullptr;
		script->OnEvent(ev);
	}

	script->onPlace({ 5, 7 });
	CHECK(bu.params.param_u8 == 98);

	return e;
}


// ---------------- Main test function ----------------

void unittest_gui_layout(int which)
{
	test_layout_table_calc();
	test_layout_flexbox_calc();

	printf("Starting layout test %i\n", which);

	UnittestEventReceiver rcv;

	core::dimension2du window_size = core::dimension2du(850, 550);

	SIrrlichtCreationParameters params;
	params.DriverType = video::EDT_OPENGL;
	params.Vsync = true;
	params.WindowSize = window_size;
	params.Stencilbuffer = false;
	params.EventReceiver = &rcv;

	device = createDeviceEx(params);

	CHECK(device);

	video::IVideoDriver *driver = device->getVideoDriver();
	gui::IGUIEnvironment *guienv = device->getGUIEnvironment();

	Element *root = nullptr;
	const wchar_t *suffix = L"INVALID";
	switch (which) {
		case 1:
			root = setup_box_demo();
			suffix = L"Box demo";
			break;
		case 2:
			root = setup_table_demo();
			suffix = L"Table demo";
			break;
		case 3:
			root = setup_tabcontrol();
			suffix = L"Tab control (Irrlicht)";
			break;
		case 4:
			root = setup_guiscript(guienv);
			suffix = L"GuiScript test";
			break;
	}
	CHECK(root);
	device->setWindowCaption((std::wstring(L"Layout demo") + L" - " + suffix).c_str());

	bool is_new_screen = true;
	while (device->run()) {
		auto screensize = driver->getScreenSize();
		if (screensize != window_size) {
			is_new_screen = true;
			window_size = screensize;
		}

		if (is_new_screen && root) {
			unittest_tic();
			if (rect_override.getArea() == 0)
				root->start({0, 0, (s16)window_size.Width, (s16)window_size.Height });
			else
				root->start({0, 0, (s16)rect_override.getWidth(), (s16)rect_override.getHeight() });
			unittest_toc("root->start()");
		}

		driver->beginScene(true, true, video::SColor(0xFF777777));

		guienv->drawAll();

		if (true) {
			// Debug information. "root" is outlined in red.
			IGUIElementWrapper::draw_wireframe(root, device, 0xFFFF0000);
		}

		driver->endScene();
		is_new_screen = false;
	}

	root->clear();
	delete script;
	guienv->clear();
	puts("GUI: Terminated properly.");
}


#else // BUILD_CLIENT

#include <stdio.h>

void unittest_gui_layout(int num)
{
	puts("Not implemented");
}


#endif
