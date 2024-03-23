
#include "unittest_internal.h"

#if BUILD_CLIENT

#include "gui/guilayout/guilayout.h"
#include "gui/guilayout/guilayout_irrlicht.h"
#include "core/utils.h"
#include <chrono>
#include <irrlicht.h> // createDeviceEx (and everything else)


using namespace irr;
using namespace guilayout;

#if 0
	#define DBG_TEST(...) printf(__VA_ARGS__)
#else
	#define DBG_TEST(...) do {} while(0)
#endif

// ------------------- Irrlicht env -------------------

static IrrlichtDevice *device;

struct UnittestEventReceiver : public IEventReceiver {
	~UnittestEventReceiver() {}

	bool OnEvent(const SEvent &event) override
	{
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

	void setSize(u16_x2 size)
	{
		min_size = size;
		m_element->setMinSize({
			min_size[0], min_size[1]
		});
	}

	void getMinSize(bool sx, bool sy) override
	{
		// min_size
		if (index_pos[0] == 1 && index_pos[1] == 0) {
			if (sx)
				setSize({40, 100});
			else if (sy)
				setSize({100, 20});
			else
				setSize({100, 100});
			DBG_TEST("Button shrink: X %i, Y %i\n", sx, sy);
		}
	}

	u16_x2 index_pos;
};

// ------------------- GUI builders -------------------

static Element *setup_table_demo()
{
	static Table root;
	root.setSize(5, 4);
	root.col(4)->weight = 4;
	root.row(0)->weight = 0;
	root.row(3)->weight = 10;
	root.margin = {10, 1, 10, 1};

	for (u16 xp = 0; xp < 4; ++xp) {
		for (u16 yp = 0; yp < 4; ++yp) {
			TestButton *b = root.add<TestButton>(xp, yp);
			b->index_pos = { xp, yp };

			switch (xp) {
				case 1: b->margin = {5,5,5,5}; break; // x center, y center
				case 2: b->margin = {15,15,15,0}; break; // y center, stick to left
				case 3: b->margin = {0,15,15,0}; break; // stick to top left
				default: b->margin = {0,0,0,0}; break; // fill
			}

			switch (yp) {
				case 1: b->setSize({150, 20}); break;
				case 2: b->setSize({200, (u16)(60 + (xp == 2) * 60)}); break;
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

			switch (xp) {
				case 1: b->margin = {5,5,5,5}; break;
				case 2: b->margin = {5,5,5,0}; break;
				case 3: b->margin = {5,0,5,0}; break;
				default: b->margin = {0,0,0,0}; break;
			}

			switch (yp) {
				case 1: b->setSize({200, 20}); break;
				case 2: b->setSize({200, (u16)(60 + (xp == 2) * 60)}); break;
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
		label->margin = {1,0,1,1};
		label->setSize({100, 30});

		auto input = hbox->add<TestButton>();
		input->margin = {1,0,0,0};
		input->expand = {1,0};
		input->setSize({200, 30});

		auto btn = hbox->add<TestButton>();
		btn->margin = {1,1,0,0};
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
	wrap->min_size = {200, (u16)(size.Height * 0.6f) };
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
		box->margin = { 2, 1, 1, 3 };
		auto btn = device->getGUIEnvironment()->addButton(norect, tab, 422, L"test");
		auto a = box->add<IGUIElementWrapper>(btn);
		a->min_size = { 50, 20 };
		a->expand = { 1, 1 };
	}

	return &main;
}

// ---------------- Main test function ----------------

void unittest_gui_layout(int which)
{
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
	}
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
			root->start(
				{0, 0},
				{
					(u16)window_size.Width,
					(u16)window_size.Height
				}
			);
			unittest_toc("root->start()");
		}

		driver->beginScene(true, true, video::SColor(0xFF000022));

		guienv->drawAll();

		if (which == 3) {
			// red rectangle to highlight area
			auto wrap = ((IGUIElementWrapper *)((FlexBox *)root)->at(0));
			wrap->debugFillArea(device->getVideoDriver(), 0xFFFF0000);
		}

		driver->endScene();
		is_new_screen = false;
	}

	root->clear();
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
