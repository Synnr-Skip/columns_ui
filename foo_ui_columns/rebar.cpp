#include "stdafx.h"
#include "rebar.h"
#include "main_window.h"

//extern HBITMAP buttons_images;

extern rebar_window * g_rebar_window;

extern cfg_rebar g_cfg_rebar;
extern cfg_band_cache_t cfg_band_cache;

constexpr const auto default_toolbar_width = 21;
constexpr const auto default_toolbar_height = 21;


void destroy_rebar(bool save_config)
{
	if (g_rebar_window)
	{
		g_rebar_window->destroy();
		if (save_config)
		{
			g_cfg_rebar.set_rebar_info(g_rebar_window->bands);
			cfg_band_cache.set_band_cache(g_rebar_window->cache);
		}
		delete g_rebar_window;
		g_rebar_window = nullptr;
		g_rebar = nullptr;
	}
}

void create_rebar()
{
	if (cfg_toolbars)
	{
		if (!g_rebar_window)
		{
			g_rebar_window = new(std::nothrow) rebar_window();
			if (g_rebar_window)
			{
				rebar_info blah;
				g_cfg_rebar.get_rebar_info(blah);
				cfg_band_cache.get_band_cache(g_rebar_window->cache);
				g_rebar = g_rebar_window->init(blah);
				if (!g_rebar)
				{
					delete g_rebar_window;
					g_rebar_window = nullptr;
				}
			}
		}
	}
	else destroy_rebar();
}


void cfg_rebar::export_config(stream_writer * p_out, t_uint32 mode, cui::fcl::t_export_feedback & feedback, abort_callback & p_abort)
{
	enum {stream_version = 0};
	p_out->write_lendian_t((t_uint32)stream_version, p_abort);

	rebar_info pentries;

	if (g_rebar_window)
		pentries.set_rebar_info(g_rebar_window->bands);
	else
		get_rebar_info(pentries);

	t_size i;
	t_size count = pentries.get_count();
	p_out->write_lendian_t(count, p_abort);
	for(i=0;i<count;i++)
	{
		feedback.add_required_panel(pentries.get_item(i)->guid);
		pentries.get_item(i)->_export(p_out, mode, p_abort);
	}
}

void cfg_rebar::import_config(stream_reader * p_reader, t_size size, t_uint32 mode, pfc::list_base_t<GUID> & panels, abort_callback & p_abort)
{
	t_uint32 version;
	rebar_info pentries;
	p_reader->read_lendian_t(version, p_abort);
	if (version > 0)
		throw exception_io_unsupported_format();
	t_size i, count;
	p_reader->read_lendian_t(count, p_abort);
	for(i=0;i<count;i++)
	{
		auto  temp = new rebar_band_info(pfc::guid_null,100);
		temp->import(p_reader, mode, p_abort);

		uie::window_ptr ptr;
		if (!uie::window::create_by_guid(temp->guid, ptr))
			panels.add_item(temp->guid);
		pentries.add_item(temp);
	}
	if (g_main_window)
		destroy_rebar();
	set_rebar_info(pentries);
	cfg_band_cache.reset();
	if (g_main_window)
	{
		create_rebar();
		if (g_rebar) 
		{
			ShowWindow(g_rebar, SW_SHOWNORMAL);
			UpdateWindow(g_rebar);
		}
		size_windows();
	}
}

void band_cache::add_entry(const GUID & guid, unsigned width)
{
	unsigned n, count = get_count();
	for (n=0;n<count;n++)
	{
		band_cache_entry & p_bce = (*this)[n];
		if (p_bce.guid == guid)
		{
			p_bce.width = width;
			return;
		}
	}
	band_cache_entry item;
	item.guid = guid;
	item.width = width;
	add_item(item);
}
unsigned band_cache::get_width(const GUID & guid)
{
	unsigned rv = 100;
	unsigned n, count = get_count();
	for (n=0;n<count;n++)
	{
		const auto & p_bce = get_item_ref(n);
		if (p_bce.guid == guid)
		{
			rv = p_bce.width;
		}
	}
	return rv;
}
void band_cache::write(stream_writer * out, abort_callback & p_abort)
{
	unsigned n, count = get_count();
	out->write_lendian_t(count, p_abort);
	for (n=0; n<count; n++)
	{
		const auto & p_bce = get_item_ref(n);
		out->write_lendian_t(p_bce.guid, p_abort);
		out->write_lendian_t(p_bce.width, p_abort);
	}
}
void band_cache::read(stream_reader * data, abort_callback & p_abort)
{
		remove_all();
		unsigned n, count;
		data->read_lendian_t(count, p_abort);
		for (n=0; n<count; n++)
		{
			GUID guid;
			unsigned width;
			data->read_lendian_t(guid, p_abort);
			data->read_lendian_t(width, p_abort);
			band_cache_entry item;
			item.guid = guid;
			item.width = width;
			add_item(item);
		}
}

void cfg_band_cache_t::get_data_raw(stream_writer * out, abort_callback & p_abort)
{
	if (g_rebar_window) entries.copy(g_rebar_window->cache);
	return entries.write(out, p_abort);
}

void cfg_band_cache_t::set_data_raw(stream_reader * p_reader, unsigned p_sizehint, abort_callback & p_abort)
{
	return entries.read(p_reader, p_abort);
}

void cfg_band_cache_t::get_band_cache(band_cache & out)
{
	out.remove_all();
	out.add_items(entries);
}
void cfg_band_cache_t::set_band_cache(band_cache & in)
{
	entries.remove_all();
	entries.add_items(in);
}
void cfg_band_cache_t::reset()
{
	entries.remove_all();
}

rebar_band_info::rebar_band_info(GUID id, unsigned h) : guid(id), /*guid2(id), */wnd(nullptr), p_ext(nullptr), width(h), rbbs_break(0) {};


void rebar_band_info::_export(stream_writer * out, t_uint32 type, abort_callback & p_abort)
{
	uie::window_ptr ptr = p_ext;
	if (!ptr.is_valid())
	{
		if (uie::window::create_by_guid(guid, ptr))
		{
			//if (type==cui::fcl::type_public)
			try {
			ptr->set_config_from_ptr(config.get_ptr(), config.get_size(), p_abort);
			} catch (const exception_io &) {};
		}
		else
			throw cui::fcl::exception_missing_panel();
	}
	stream_writer_memblock w;
	if (type==cui::fcl::type_public)
		ptr->export_config(&w, p_abort);
	else
		ptr->get_config(&w, p_abort);
	out->write_lendian_t(guid, p_abort);
	out->write_lendian_t(width.getScaledValue(), p_abort);
	out->write_lendian_t(rbbs_break, p_abort);
	DWORD size = w.m_data.get_size();
	out->write_lendian_t(size, p_abort);
	out->write(w.m_data.get_ptr(), size, p_abort);
}

void rebar_band_info::import(stream_reader * r, t_uint32 type, abort_callback & p_abort)
{
	if (p_ext.is_valid())
		throw pfc::exception_bug_check();
	r->read_lendian_t(guid, p_abort);
	uint32_t width_;
	r->read_lendian_t(width_, p_abort);
	width = width_;
	r->read_lendian_t(rbbs_break, p_abort);
	unsigned mem_size;
	r->read_lendian_t(mem_size, p_abort);

	if (mem_size)
	{
		pfc::array_t<t_uint8> data;
		data.set_size(mem_size);
		r->read(data.get_ptr(), mem_size, p_abort);

		if (type==cui::fcl::type_public)
		{
			uie::window_ptr ptr;

			if (uie::window::create_by_guid(guid, ptr))
			{
				stream_reader_memblock_ref reader(data.get_ptr(), data.get_size());
				ptr->import_config(&reader, data.get_size(), p_abort);
				stream_writer_memblock_ref writer(config, true);
				ptr->get_config(&writer, p_abort);
			}
		}
		else config = data;
	}
}

void rebar_band_info::write(stream_writer * out, abort_callback & p_abort)
{
		out->write_lendian_t(guid, p_abort);
		out->write_lendian_t(width.getScaledValue(), p_abort);
		out->write_lendian_t(rbbs_break, p_abort);
		DWORD size = config.get_size();
		out->write_lendian_t(size, p_abort);
		out->write(config.get_ptr(), size, p_abort);
}

void rebar_band_info::read(stream_reader * r, abort_callback & p_abort)
{
		r->read_lendian_t(guid, p_abort);
		uint32_t width_;
		r->read_lendian_t(width_, p_abort);
		width = width_;
		r->read_lendian_t(rbbs_break, p_abort);

		unsigned mem_size;
		r->read_lendian_t(mem_size, p_abort);

		if (mem_size)
		{
			config.set_size(mem_size);
			r->read(config.get_ptr(), mem_size, p_abort);
		}
	
}

void rebar_band_info::write_extra(stream_writer * out, abort_callback & p_abort)
{
	out->write_lendian_t(width.value, p_abort);
	out->write_lendian_t(width.dpi, p_abort);
}

void rebar_band_info::read_extra(stream_reader * r, abort_callback & p_abort)
{
	r->read_lendian_t(width.value, p_abort);
	r->read_lendian_t(width.dpi, p_abort);
}

void rebar_band_info::copy(rebar_band_info & out)
{
	out.guid = guid;
	//		out.guid2 = guid;
	out.width = width;
	out.rbbs_break = rbbs_break;
	out.config.set_size(0);
	if (wnd && p_ext.is_valid())
	{
		try {
			abort_callback_dummy abortCallbackDummy;
			stream_writer_memblock_ref writer(config);
			config.set_size(0);
			p_ext->get_config(&writer, abortCallbackDummy);
		} catch (const pfc::exception &) {};
	}
	out.config.append_fromptr(config.get_ptr(), config.get_size());
}

void rebar_info::set_rebar_info(rebar_info & in)
{
	delete_all();
	unsigned n, count = in.get_count();
	for (n=0; n<count; n++)
	{
		auto  item = new(std::nothrow) rebar_band_info(pfc::guid_null, 100);
		if (item)
		{
			in[n]->copy(*item);
			add_item(item);
		}
	}
}
rebar_band_info * rebar_info::find_by_wnd(HWND wnd)
{
	unsigned count = get_count(),n;
	rebar_band_info * rv = nullptr;
	for (n=0; n<count;n++)
	{
		if (wnd == get_item(n)->wnd) {rv = get_item(n); break;}
	}
	return rv;
}
unsigned rebar_info::find_by_wnd_n(HWND wnd)
{
	unsigned count = get_count(),n;
	unsigned rv = -1;
	for (n=0; n<count;n++)
	{
		if (wnd == get_item(n)->wnd) {rv = n; break;}
	}
	return rv;
}
void rebar_info::add_band(const GUID & id, unsigned width, bool new_line, const ui_extension::window_ptr & p_ext)
{
	auto  item = new(std::nothrow) rebar_band_info(id, width);
	if (item)
	{
		item->rbbs_break = new_line;
		item->p_ext = p_ext;
		add_item(item);
	}
}

void rebar_info::insert_band(unsigned idx,const GUID & id, unsigned width, bool new_line, const ui_extension::window_ptr & p_ext)
{
	if (idx <= get_count())
	{
		auto  item = new(std::nothrow) rebar_band_info(id, width);
		if (item)
		{
			item->rbbs_break = new_line;
			item->p_ext = p_ext;
			insert_item(item, idx);
		}
	}
}

void cfg_rebar::get_data_raw(stream_writer * out, abort_callback & p_abort)
{
	if (g_rebar_window)
		entries.set_rebar_info(g_rebar_window->bands);

	uint32_t num = pfc::downcast_guarded<uint32_t>(entries.get_count());
	out->write_lendian_t(num, p_abort);
	for(uint32_t n=0;n<num;n++)
	{
		entries.get_item(n)->write(out, p_abort);
	}
	
	// Extra data added in version 0.5.0
	out->write_lendian_t(static_cast<uint32_t>(StreamVersion::VersionCurrent), p_abort);

	for (t_size n = 0; n < num; n++) {
		stream_writer_memblock extraData;
		entries.get_item(n)->write_extra(&extraData, p_abort);
		out->write_lendian_t(pfc::downcast_guarded<uint32_t>(extraData.m_data.get_size()), p_abort);
		out->write(extraData.m_data.get_ptr(), extraData.m_data.get_size(), p_abort);
	}
}

void cfg_rebar::set_data_raw(stream_reader * p_reader, unsigned p_sizehint, abort_callback & p_abort)
{
	entries.delete_all();
	
	uint32_t itemCount;
	p_reader->read_lendian_t(itemCount, p_abort);

	for (uint32_t i = 0; i < itemCount; i++)
	{
		auto item = std::make_unique<rebar_band_info>(pfc::guid_null, 100);
		item->read(p_reader, p_abort);
		entries.add_item(item.release());
	}

	// Extra data added in version 0.5.0
	StreamVersion streamVersion = StreamVersion::Version0;
	try {
		uint32_t streamVersion_;
		p_reader->read_lendian_t(streamVersion_, p_abort);
		streamVersion = static_cast<StreamVersion>(streamVersion_);
	} catch (const exception_io_data_truncation &) {
	}

	if (streamVersion >= StreamVersion::Version1) {
		for (uint32_t i = 0; i < itemCount; i++) {
			uint32_t extraDataSize;
			p_reader->read_lendian_t(extraDataSize, p_abort);
			pfc::array_staticsize_t<t_uint8> columnExtraData(extraDataSize);
			p_reader->read(columnExtraData.get_ptr(), columnExtraData.get_size(), p_abort);
			stream_reader_memblock_ref columnReader(columnExtraData);
			entries[i]->read_extra(&columnReader, p_abort);
		}
	}
}

void cfg_rebar::get_rebar_info(rebar_info & out)
{
	out.delete_all();
	unsigned n, count = entries.get_count();
	for (n=0; n<count; n++)
	{
		auto  item = new(std::nothrow) rebar_band_info(pfc::guid_null,100);
		if (item)
		{
			entries[n]->copy(*item);
			out.add_item(item);
		}
	}
}
void cfg_rebar::set_rebar_info(rebar_info & in)
{
	entries.delete_all();
	unsigned n, count = in.get_count();
	for (n=0; n<count; n++)
	{
		auto  item = new(std::nothrow) rebar_band_info(pfc::guid_null,100);
		if (item)
		{
			in[n]->copy(*item);
			entries.add_item(item);
		}
	}
}
void cfg_rebar::reset()
{
	entries.delete_all();
	{
		//menubar
		entries.add_band(cui::toolbars::guid_menu, 9999);
	}
	{
		//buttons
		entries.add_band(cui::toolbars::guid_buttons, 100, true);
	}
	{
		//seekbar
		entries.add_band(cui::toolbars::guid_seek_bar, 9999);
	}
	{
		//order
		entries.add_band(cui::toolbars::guid_playback_order, 100);
	}
	{
		//vis
		entries.add_band(cui::toolbars::guid_spectrum_analyser, 125);
	}
}

// {3D3C8D68-3AB9-4ad5-A4FA-22427ABAEBF4}
static const GUID rebar_guid = 
{ 0x3d3c8d68, 0x3ab9, 0x4ad5, { 0xa4, 0xfa, 0x22, 0x42, 0x7a, 0xba, 0xeb, 0xf4 } };


class ui_ext_host_rebar : public ui_extension::window_host_with_control
{
public:

	void get_name(pfc::string_base & out)const override
	{
		out.set_string("Columns UI/Toolbars");
	};

	bool is_available()const override
	{
		return g_rebar_window != nullptr;
	}

	unsigned get_supported_types()const override
	{
		return ui_extension::type_toolbar;
	}

	void insert_extension (const GUID & in, unsigned height, unsigned width) override
	{
		if (g_rebar_window)
		{
			 g_rebar_window->add_band(in, width);
		}
	};

	void insert_extension (ui_extension::window_ptr & p_ext, unsigned height, unsigned width) override
	{
		if (g_rebar_window)
		{
			g_rebar_window->add_band(p_ext->get_extension_guid(), width, p_ext);
		}
	};

	unsigned is_resize_supported(HWND wnd)const override
	{
		return ui_extension::size_width;
	}

	bool request_resize(HWND wnd, unsigned flags, unsigned width, unsigned height) override
	{
		if ((flags & ui_extension::size_width) && !(flags & ui_extension::size_height) && g_rebar_window)
		{
			unsigned idx = g_rebar_window->bands.find_by_wnd_n(wnd);
			if (idx < g_rebar_window->bands.get_count())
			{
				g_rebar_window->bands[idx]->width = width;
				g_rebar_window->update_band(idx, true);
				return true;
		/*		REBARBANDINFO  rbbi;
				memset(&rbbi,0,sizeof(rbbi));

				rbbi.cbSize = sizeof(rbbi);
				rbbi.fMask = RBBIM_SIZE;
				rbbi.cx = width;
				uRebar_InsertItem(wnd, idx, &rbbi, false);*/
			}
		}
		return false;
	}
	bool is_visible(HWND wnd)const override
	{
		return true;
	}
	bool is_visibility_modifiable(HWND wnd, bool desired_visibility)const override
	{
		return false;
	}
	bool set_window_visibility(HWND wnd, bool visibility) override
	{
		return false;
	}

	void on_size_limit_change(HWND wnd, unsigned flags) override
	{
		if (g_rebar_window)
		{
			rebar_band_info * p_ext = g_rebar_window->bands.find_by_wnd(wnd);
			if (p_ext)
			{
				g_rebar_window->update_band(wnd);
			}
		}
	};

	const GUID & get_host_guid()const override{return rebar_guid;}

	bool override_status_text_create(service_ptr_t<ui_status_text_override> & p_out) override
	{
		static_api_ptr_t<ui_control> api;
		return api->override_status_text_create(p_out);
	}
	
	virtual bool on_key(UINT msg, LPARAM lp, WPARAM wp, bool process_keyboard_shortcuts) 
	{
		return process_keydown(msg, lp, wp, false, process_keyboard_shortcuts);
	};

	void relinquish_ownership(HWND wnd) override
	{
		if (g_rebar_window)
		{
			g_rebar_window->delete_band(wnd, false);
		}
	};

};

ui_extension::window_host_factory_single<ui_ext_host_rebar> g_ui_ext_host_rebar;


rebar_window::rebar_window() : wnd_rebar(nullptr) {};
rebar_window::~rebar_window() = default;;

HWND rebar_window::init(rebar_info & new_bands)
{
	HWND rv = nullptr;
	
	bands.set_rebar_info(new_bands);
	
	if (!wnd_rebar) 
	{
		rv = wnd_rebar = CreateWindowEx( WS_EX_TOOLWINDOW|WS_EX_CONTROLPARENT, REBARCLASSNAME, nullptr, WS_BORDER |  WS_CHILD | 
			WS_CLIPCHILDREN |  WS_CLIPSIBLINGS |  RBS_VARHEIGHT |  RBS_DBLCLKTOGGLE | RBS_AUTOSIZE |
			RBS_BANDBORDERS |  CCS_NODIVIDER |  CCS_NOPARENTALIGN | 0, 0, 0, 0, 0, 
			g_main_window,  (HMENU)ID_REBAR,  core_api::get_my_instance(),  nullptr);
		//SetWindowTheme(wnd_rebar, L"Default", NULL);
	}

	refresh_bands();
	
	return rv;
}

bool rebar_window::on_menu_char (unsigned short c)
{
	bool rv = false;
	unsigned n, count = bands.get_count();

	for (n=0; n<count; n++)
	{
		HWND wnd = bands[n]->wnd;
		ui_extension::window_ptr p_ext = bands[n]->p_ext;
		if (p_ext.is_valid() && wnd)
		{
			service_ptr_t<uie::menu_window> p_menu_ext;
			if (p_ext->service_query_t(p_menu_ext))
			{
				rv = p_menu_ext->on_menuchar(c);
				if (rv) break;
			}
		}
	}
	return rv;

}

void rebar_window::show_accelerators ()
{
	unsigned n, count = bands.get_count();

	for (n=0; n<count; n++)
	{
		HWND wnd = bands[n]->wnd;
		ui_extension::window_ptr p_ext = bands[n]->p_ext;
		if (p_ext.is_valid() && wnd)
		{
			service_ptr_t<uie::menu_window> p_menu_ext;
			if (p_ext->service_query_t(p_menu_ext))
			{
				p_menu_ext->show_accelerators();
			}
		}
	}

}

void rebar_window::hide_accelerators ()
{
	unsigned n, count = bands.get_count();

	{

		for (n=0; n<count; n++)
		{
			HWND wnd = bands[n]->wnd;
			ui_extension::window_ptr p_ext = bands[n]->p_ext;
			if (p_ext.is_valid() && wnd)
			{
				service_ptr_t<uie::menu_window> p_menu_ext;
				if (p_ext->service_query_t(p_menu_ext))
				{
					p_menu_ext->hide_accelerators();
				}
			}
		}
	}

}

bool rebar_window::is_menu_focused()
{
	unsigned n, count = bands.get_count();

	for (n=0; n<count; n++)
	{
		HWND wnd = bands[n]->wnd;
		ui_extension::window_ptr p_ext = bands[n]->p_ext;
		if (p_ext.is_valid() && wnd)
		{
			service_ptr_t<uie::menu_window> p_menu_ext;
			if (p_ext->service_query_t(p_menu_ext))
			{
				if (p_menu_ext->is_menu_focused()) return true;
			}
		}
	}
	return false;
}

bool rebar_window::get_previous_menu_focus_window(HWND & wnd_previous) const
{
	t_size n, count = bands.get_count();

	for (n = 0; n<count; n++)
	{
		HWND wnd = bands[n]->wnd;
		ui_extension::window_ptr p_ext = bands[n]->p_ext;
		if (p_ext.is_valid() && wnd)
		{
			service_ptr_t<uie::menu_window_v2> p_menu_ext;
			if (p_ext->service_query_t(p_menu_ext))
			{
				if (p_menu_ext->is_menu_focused()) {
					wnd_previous = p_menu_ext->get_previous_focus_window();
					return true;
				}
			}
		}
	}
	return false;
}

bool rebar_window::set_menu_focus()
{
	unsigned n, count = bands.get_count();
	bool rv = false;

	for (n=0; n<count; n++)
	{
		HWND wnd = bands[n]->wnd;
		ui_extension::window_ptr p_ext = bands[n]->p_ext;
		if (p_ext.is_valid() && wnd)
		{
			service_ptr_t<uie::menu_window> p_menu_ext;
			if (p_ext->service_query_t(p_menu_ext))
			{
				if (!rv)
				{
					p_menu_ext->set_focus();
					rv = true;
				}
				else
				{
					p_menu_ext->hide_accelerators();
				}
				
			}
		}
	}
	return rv;

}

void rebar_window::on_themechanged()
{
	SetWindowPos(wnd_rebar, nullptr, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOZORDER|SWP_NOSIZE|SWP_FRAMECHANGED);
	RedrawWindow(wnd_rebar, nullptr, nullptr, RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN);
}

void rebar_window::save_bands()
{
	REBARBANDINFO rbbi;
	memset(&rbbi,0,sizeof(rbbi));

	rbbi.cbSize = REBARBANDINFOW_V6_SIZE;
	rbbi.fMask = RBBIM_SIZE|RBBIM_STYLE|RBBIM_LPARAM;

	unsigned band_count = bands.get_count();
	pfc::array_t<unsigned> order;
	order.set_size(band_count);

	UINT count  = SendMessage(wnd_rebar, RB_GETBANDCOUNT, 0, 0);

	bool b_death = false;

	if (count && band_count == count)
	{
		unsigned n;
		for (n=0;n<count;n++)
		{
			BOOL b_OK = SendMessage(wnd_rebar, RB_GETBANDINFO , n, (LPARAM)&rbbi);
			if (b_OK && (unsigned)rbbi.lParam < count) 
			{
				order[n] = rbbi.lParam;
				bands[rbbi.lParam]->width = rbbi.cx;
				bands[rbbi.lParam]->rbbs_break = ((rbbi.fStyle & RBBS_BREAK) !=0);
			} else b_death = true;
		}

		if (!b_death)
			bands.reorder(order.get_ptr());
	//	if (update) 
			refresh_bands(false,false);
	}
}

bool rebar_window::check_band(const GUID & id)
{
	bool rv = false;
	unsigned n, count = bands.get_count();
	for (n=0; n<count; n++)
	{
		if (bands[n]->guid == id)
		{
			rv = true;
			break;
		}
	}
	return rv;
}

bool rebar_window::find_band(const GUID & id, unsigned & out)
{
	bool rv = false;
	unsigned n, count = bands.get_count();
	for (n=0; n<count; n++)
	{
		if (bands[n]->guid == id)
		{
			out = n;
			rv = true;
			break;
		}
	}
	return rv;
}

bool rebar_window::delete_band(const GUID & id)
{
	unsigned n=0;
	bool rv = find_band(id, n);
	if (rv) delete_band(n);
	return rv;
}

void rebar_window::destroy_bands()
{
//	save_bands(false);
	abort_callback_dummy abortCallbackDummy;

	UINT count  = SendMessage(wnd_rebar, RB_GETBANDCOUNT, 0, 0);

	if (count>0 && count == bands.get_count())
	{
		unsigned n;
		for (n=0;count;n++,count--)
		{
			SendMessage(wnd_rebar, RB_SHOWBAND , 0, FALSE);
			SendMessage(wnd_rebar, RB_DELETEBAND, 0, 0);
			ui_extension::window_ptr p_ext = bands[n]->p_ext;
			if (p_ext.is_valid())
			{
				bands[n]->config.set_size(0);
				stream_writer_memblock_ref data(bands[n]->config);
				try {
					p_ext->get_config(&data, abortCallbackDummy);
				}
				catch (const pfc::exception &) {};
				p_ext->destroy_window();
				bands[n]->wnd=nullptr;
				p_ext.release();
			}
		}
	}
}

void rebar_window::destroy()
{
	destroy_bands();
	DestroyWindow(wnd_rebar);
	wnd_rebar=nullptr;
}

void rebar_window::update_bands()
{
	refresh_bands(false);
	uih::Rebar_ShowAllBands(wnd_rebar);
}

void rebar_window::delete_band(unsigned n)
{
//	save_bands();
	if (n < bands.get_count())
	{
		SendMessage(wnd_rebar, RB_SHOWBAND , n, FALSE);
		SendMessage(wnd_rebar, RB_DELETEBAND, n, 0);
		ui_extension::window_ptr p_ext = bands[n]->p_ext;
		if (p_ext.is_valid())
		{
			p_ext->destroy_window();
			p_ext.release();
		}
		cache.add_entry(bands[n]->guid, bands[n]->width);
		bands.delete_by_idx(n);
		refresh_bands(false, false);
	}
}

void rebar_window::delete_band(HWND wnd, bool destroy)
{
//	save_bands();
	unsigned n = bands.find_by_wnd_n(wnd);
	if (n < bands.get_count())
	{
		SendMessage(wnd_rebar, RB_SHOWBAND , n, FALSE);
		SendMessage(wnd_rebar, RB_DELETEBAND, n, 0);
		ui_extension::window_ptr p_ext = bands[n]->p_ext;
		if (p_ext.is_valid())
		{
			if (destroy) p_ext->destroy_window();
			p_ext.release();
		}
		cache.add_entry(bands[n]->guid, bands[n]->width);
		bands.delete_by_idx(n);
		refresh_bands(false, false);

	}
}

void rebar_window::add_band(const GUID & guid, unsigned width, const ui_extension::window_ptr & p_ext)
{
//	save_bands();
	bands.add_band(guid, width, false, p_ext);
	refresh_bands(false, false);
}

void rebar_window::insert_band(unsigned idx, const GUID & guid, unsigned width, const ui_extension::window_ptr & p_ext)
{
//	save_bands();
	bands.insert_band(idx, guid, width, false, p_ext);
	refresh_bands(false, false);
}

void rebar_window::update_band(HWND wnd, bool size/*, bool min_height, bool max_height, bool min_width, bool max_width*/)
{
	unsigned n, count = bands.get_count();
	for (n=0; n<count;n++)
	{
		if (bands[n]->wnd == wnd)
		{
			update_band(n, size);
		}

	}

}

void rebar_window::update_band(unsigned n, bool size/*, bool min_height, bool max_height, bool min_width, bool max_width*/)
{
	ui_extension::window_ptr p_ext = bands[n]->p_ext;
	if (p_ext.is_valid())
	{
		uREBARBANDINFO  rbbi;
		memset(&rbbi,0,sizeof(rbbi));
		rbbi.cbSize       = sizeof(uREBARBANDINFO);
		
		rbbi.fMask        |= RBBIM_CHILDSIZE;
		
		MINMAXINFO mmi;
		memset(&mmi, 0, sizeof(MINMAXINFO));
		mmi.ptMaxTrackSize.x = MAXLONG;
		mmi.ptMaxTrackSize.y = MAXLONG;
		SendMessage(bands[n]->wnd, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&mmi));
		
		if (mmi.ptMaxTrackSize.y < 0)
			mmi.ptMaxTrackSize.y = 0;
		if (mmi.ptMinTrackSize.y <= 0)
			mmi.ptMinTrackSize.y = min(uih::ScaleDpiValue(default_toolbar_height), mmi.ptMaxTrackSize.y);
		if (mmi.ptMinTrackSize.x <= 0)
			mmi.ptMinTrackSize.x = uih::ScaleDpiValue(default_toolbar_width);

		rbbi.cyMinChild = mmi.ptMinTrackSize.y;
		rbbi.cyMaxChild = mmi.ptMaxTrackSize.y;
		rbbi.cxMinChild = mmi.ptMinTrackSize.x;
		
		if (size)
		{
			rbbi.fMask |= RBBIM_SIZE;
			rbbi.cx = bands[n]->width;
		}

		uRebar_InsertItem(wnd_rebar, n, &rbbi, false);
		SendMessage(wnd_rebar, RB_SHOWBAND, n, TRUE);
		
	}
}

void rebar_window::refresh_bands(bool force_destroy_bands, bool save)
{
	abort_callback_dummy abortCallbackDummy;

	if (force_destroy_bands) destroy_bands();
//	else if (save) save_bands(false);
	
	unsigned n, count = bands.get_count();
	for (n=0; n<count;)
	{
		
		bool adding = false;
		HWND wnd_band=bands[n]->wnd;
		uREBARBANDINFO  rbbi;
		memset(&rbbi,0,sizeof(rbbi));
		rbbi.cbSize       = sizeof(uREBARBANDINFO);

		
		if (!wnd_band)
		{
			ui_extension::window_ptr p_ext = bands[n]->p_ext;
			bool b_new = false;
			if (!p_ext.is_valid()) 
			{
				ui_extension::window::create_by_guid(bands[n]->guid,p_ext);
				b_new = true;
			}

			if (p_ext.is_valid() && p_ext->is_available(&g_ui_ext_host_rebar.get_static_instance()))
			{

				adding =true;
				if (b_new)
				{
					try
					{
						p_ext->set_config_from_ptr(bands[n]->config.get_ptr(), bands[n]->config.get_size(), abortCallbackDummy);
					}
					catch (const exception_io & e)
					{
						console::formatter formatter;
						formatter << "Error setting panel config: " << e.what();
					}
				}
				wnd_band = p_ext->create_or_transfer_window(wnd_rebar, ui_extension::window_host_ptr(&g_ui_ext_host_rebar.get_static_instance()));
				if (wnd_band)
				{
					bands[n]->p_ext = p_ext;
					bands[n]->wnd = wnd_band;
					ShowWindow(wnd_band, SW_SHOWNORMAL);
					
					rbbi.fMask |= RBBIM_CHILDSIZE;

					MINMAXINFO mmi;
					memset(&mmi, 0, sizeof(MINMAXINFO));
					mmi.ptMaxTrackSize.x = MAXLONG;
					mmi.ptMaxTrackSize.y = MAXLONG;
					SendMessage(wnd_band, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&mmi));

					if (mmi.ptMaxTrackSize.y < 0)
						mmi.ptMaxTrackSize.y = 0;
					if (mmi.ptMinTrackSize.y <= 0)
						mmi.ptMinTrackSize.y = min(uih::ScaleDpiValue(default_toolbar_height), mmi.ptMaxTrackSize.y);
					if (mmi.ptMinTrackSize.x <= 0)
						mmi.ptMinTrackSize.x = uih::ScaleDpiValue(default_toolbar_width);

					rbbi.cyMinChild   = mmi.ptMinTrackSize.y;
					rbbi.cyMaxChild   = mmi.ptMaxTrackSize.y;
					rbbi.cxMinChild   = mmi.ptMinTrackSize.x;
				}
				else
				{
					p_ext.release();
				}
			}
			else
			{
				p_ext.release();
			}
		}



		if (wnd_band)
		{
			rbbi.fMask |= RBBIM_SIZE | RBBIM_CHILD | RBBIM_HEADERSIZE | RBBIM_LPARAM | RBBIM_STYLE;
			//rbbi.cyIntegral = 1;
			rbbi.cx = bands[n]->width;
			rbbi.fStyle = /*RBBS_VARIABLEHEIGHT|*/RBBS_CHILDEDGE | RBBS_GRIPPERALWAYS | (bands[n]->rbbs_break ? RBBS_BREAK : 0 )| (cfg_lock ? RBBS_NOGRIPPER: 0);
			rbbi.lParam = n;
			rbbi.hwndChild = wnd_band;
			rbbi.cxHeader = cfg_lock ? 5 : 9;

			uRebar_InsertItem(wnd_rebar, n, &rbbi, adding);
			
			n++;
		}
		else
		{
			bands.delete_by_idx(n);
			count--;
		}
	}
}



ui_extension::window_host & get_rebar_host()
{
	return g_ui_ext_host_rebar.get_static_instance();
}