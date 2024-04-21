
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
	root.col(4)->weight = 40;
	root.row(0)->weight = 0;
	root.row(3)->weight = 100;
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
		label->margin = {10,0,10,10};
		label->setSize({100, 30});

		auto input = hbox->add<TestButton>();
		input->margin = {1,0,0,0};
		input->expand = {1,0};
		input->setSize({200, 30});

		auto btn = hbox->add<TestButton>();
		btn->margin = {10,10,0,0};
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

// -------------- Basic calculation test --------------

class NopElement : public Element {
public:
	void updatePosition() override
	{
		// nop
	}
};

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
	main.start({0, 0}, {WIDTH, HEIGHT});

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
	main.start({0, 0}, {WIDTH, HEIGHT});

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
			IGUIElementWrapper::debugFillArea(root, device->getVideoDriver(), 0xFFFF0000);
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
