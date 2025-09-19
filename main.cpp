// main.cpp
// Compile with (MSYS2 / MinGW64):
// g++ main.cpp QrCode.cpp -o qr_gui `pkg-config --cflags --libs gtkmm-4.0 cairomm-1.0 gdk-pixbuf-2.0` -std=c++17

#include <gtkmm.h>
#include <cairomm/cairomm.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/general.h>

#include <gdk/gdkcairo.h>   // gdk_cairo_set_source_pixbuf
#include <cairo/cairo.h>

#include <string>
#include <sstream>
#include <iostream>
#include <cmath>

#include "QrCode.hpp" // Nayuki QrCode.hpp / QrCode.cpp

using qrcodegen::QrCode;

#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif

class QRWindow : public Gtk::Window {
public:
    QRWindow() {
        set_title("QR Code Generator - GTKmm 4");
        set_default_size(700, 920);
        set_resizable(true);

        container.set_margin(10);
        container.set_spacing(8);
        set_child(container);

        // Mode selector
        mode_combo.append("Text / URL");
        mode_combo.append("Wi-Fi");
        mode_combo.append("vCard");
        mode_combo.append("Image URL");
        mode_combo.append("Location");
        mode_combo.set_active_text("Text / URL");
        mode_combo.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::on_mode_changed));
        container.append(mode_combo);

        // Generic text entry (live)
        entry.set_placeholder_text("Enter text or URL...");
        entry.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        container.append(entry);

        // Wi-Fi inputs (live)
        wifi_box.set_orientation(Gtk::Orientation::VERTICAL);
        wifi_ssid.set_placeholder_text("SSID (network name)");
        wifi_ssid.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        wifi_pass.set_placeholder_text("Password");
        wifi_pass.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        wifi_enc.append("WPA/WPA2"); wifi_enc.append("WEP"); wifi_enc.append("None");
        wifi_enc.set_active_text("WPA/WPA2");
        wifi_enc.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        wifi_box.append(wifi_ssid);
        wifi_box.append(wifi_pass);
        wifi_box.append(wifi_enc);
        wifi_box.set_visible(false);
        container.append(wifi_box);

        // vCard inputs (live)
        vcard_box.set_orientation(Gtk::Orientation::VERTICAL);
        vcard_name.set_placeholder_text("Full name (FN)");
        vcard_name.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        vcard_tel.set_placeholder_text("Phone");
        vcard_tel.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        vcard_email.set_placeholder_text("Email");
        vcard_email.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        vcard_box.append(vcard_name);
        vcard_box.append(vcard_tel);
        vcard_box.append(vcard_email);
        vcard_box.set_visible(false);
        container.append(vcard_box);

        // Location inputs (live)
        location_box.set_orientation(Gtk::Orientation::VERTICAL);
        location_lat.set_placeholder_text("Latitude (e.g. -6.200000)");
        location_lat.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        location_lon.set_placeholder_text("Longitude (e.g. 106.816666)");
        location_lon.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        location_box.append(location_lat);
        location_box.append(location_lon);
        location_box.set_visible(false);
        container.append(location_box);

        // Color & shape (live)
        container.append(*Gtk::make_managed<Gtk::Label>("Module Color:"));
        color_button.set_rgba(Gdk::RGBA("black"));
        color_button.signal_color_set().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        container.append(color_button);

        container.append(*Gtk::make_managed<Gtk::Label>("Module Shape:"));
        shape_combo.append("Square"); shape_combo.append("Circle"); shape_combo.append("Rounded");
        shape_combo.set_active_text("Square");
        shape_combo.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        container.append(shape_combo);

        // Logo controls
        logo_btn.set_label("Select Logo...");
        logo_btn.signal_clicked().connect(sigc::mem_fun(*this, &QRWindow::on_select_logo));
        container.append(logo_btn);

        logo_enable.set_label("Enable Logo");
        logo_enable.signal_toggled().connect(sigc::mem_fun(*this, &QRWindow::update_qr));
        container.append(logo_enable);

        logo_size_adjustment = Gtk::Adjustment::create(25.0, 5.0, 60.0, 1.0, 5.0);
        logo_size_spin.set_adjustment(logo_size_adjustment);
        logo_size_spin.set_digits(0);
        logo_size_spin.signal_value_changed().connect(sigc::mem_fun(*this, &QRWindow::update_qr));

        auto logo_row = Gtk::Box(Gtk::Orientation::HORIZONTAL);
        logo_row.set_spacing(6);
        logo_row.append(*Gtk::make_managed<Gtk::Label>("Logo size (%):"));
        logo_row.append(logo_size_spin);
        container.append(logo_row);

        // Save button
        btn_save.set_label("Save as PNG");
        btn_save.signal_clicked().connect(sigc::mem_fun(*this, &QRWindow::on_save_png));
        container.append(btn_save);

        // Drawing area (flexible)
        drawing.set_hexpand(true);
        drawing.set_vexpand(true);
        drawing.set_draw_func(sigc::mem_fun(*this, &QRWindow::on_draw));
        container.append(drawing);

        // defaults
        last_content = "https://raymii.org";
        qr = QrCode::encodeText(last_content.c_str(), QrCode::Ecc::MEDIUM);
    }

private:
    // UI widgets
    Gtk::Box container{Gtk::Orientation::VERTICAL};
    Gtk::ComboBoxText mode_combo;
    Gtk::Entry entry;

    Gtk::Box wifi_box{Gtk::Orientation::VERTICAL};
    Gtk::Entry wifi_ssid, wifi_pass;
    Gtk::ComboBoxText wifi_enc;

    Gtk::Box vcard_box{Gtk::Orientation::VERTICAL};
    Gtk::Entry vcard_name, vcard_tel, vcard_email;

    Gtk::Box location_box{Gtk::Orientation::VERTICAL};
    Gtk::Entry location_lat, location_lon;

    Gtk::ColorButton color_button;
    Gtk::ComboBoxText shape_combo;

    Gtk::Button logo_btn, btn_save;
    Gtk::CheckButton logo_enable;
    Glib::RefPtr<Gtk::Adjustment> logo_size_adjustment;
    Gtk::SpinButton logo_size_spin;
    Glib::RefPtr<Gdk::Pixbuf> logo_pixbuf;

    Gtk::DrawingArea drawing;

    // QR state
    QrCode qr = QrCode::encodeText(" ", QrCode::Ecc::LOW);
    std::string last_content;

    // --- helpers ---
    static std::string escape_semicolons(const std::string &s) {
        std::string out; out.reserve(s.size()*2);
        for (char c : s) {
            if (c == '\\' || c == ';' || c == ',') { out.push_back('\\'); out.push_back(c); }
            else out.push_back(c);
        }
        return out;
    }
    static std::string escape_vcard(const std::string &s) {
        std::string out; out.reserve(s.size());
        for (char c : s) {
            if (c == '\n') out += "\\n"; else out.push_back(c);
        }
        return out;
    }

    // Build content string from current inputs
    std::string build_content_from_inputs() {
        auto mode = mode_combo.get_active_text();
        if (mode == "Text / URL") {
            return entry.get_text();
        } else if (mode == "Wi-Fi") {
            std::string ssid = wifi_ssid.get_text();
            if (ssid.empty()) return std::string();
            std::string pass = wifi_pass.get_text();
            std::string enc = wifi_enc.get_active_text();
            std::ostringstream ss;
            if (enc == "None") ss << "WIFI:T:nopass;S:" << escape_semicolons(ssid) << ";;";
            else ss << "WIFI:T:" << enc << ";S:" << escape_semicolons(ssid) << ";P:" << escape_semicolons(pass) << ";;";
            return ss.str();
        } else if (mode == "vCard") {
            std::string name = vcard_name.get_text();
            if (name.empty()) return std::string();
            std::ostringstream ss;
            ss << "BEGIN:VCARD\nVERSION:3.0\nFN:" << escape_vcard(name) << "\n";
            if (!vcard_tel.get_text().empty()) ss << "TEL:" << escape_vcard(vcard_tel.get_text()) << "\n";
            if (!vcard_email.get_text().empty()) ss << "EMAIL:" << escape_vcard(vcard_email.get_text()) << "\n";
            ss << "END:VCARD";
            return ss.str();
        } else if (mode == "Location") {
            std::string lat = location_lat.get_text();
            std::string lon = location_lon.get_text();
            if (lat.empty() || lon.empty()) return std::string();
            return "https://www.google.com/maps?q=" + lat + "," + lon;
        } else if (mode == "Image URL") {
            return entry.get_text();
        }
        return std::string();
    }

    // --- NEW: update_qr (live preview) ---
    void update_qr() {
        try {
            std::string content = build_content_from_inputs();
            if (content.empty()) {
                // keep previous QR if inputs incomplete, but you can also choose to clear
                return;
            }
            last_content = content;
            qr = QrCode::encodeText(last_content.c_str(), QrCode::Ecc::MEDIUM);
            drawing.queue_draw();
        } catch (const std::exception &ex) {
            // don't crash preview on invalid input; show in console
            std::cerr << "QR encode error: " << ex.what() << std::endl;
        }
    }

    // mode changed -> show/hide inputs and update preview
    void on_mode_changed() {
        std::string mode = mode_combo.get_active_text();
        entry.set_visible(mode == "Text / URL" || mode == "Image URL");
        wifi_box.set_visible(mode == "Wi-Fi");
        vcard_box.set_visible(mode == "vCard");
        location_box.set_visible(mode == "Location");
        update_qr();
    }

    // select logo (async dialog)
    void on_select_logo() {
        auto *dialog = new Gtk::FileChooserDialog("Select Logo", Gtk::FileChooser::Action::OPEN);
        dialog->set_transient_for(*this);
        dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
        dialog->add_button("_Open", Gtk::ResponseType::OK);

        auto filter = Gtk::FileFilter::create();
        filter->set_name("Images");
        filter->add_mime_type("image/png");
        filter->add_mime_type("image/jpeg");
        filter->add_mime_type("image/webp");
        dialog->add_filter(filter);

        dialog->signal_response().connect([this, dialog](int response){
            if (response == Gtk::ResponseType::OK) {
                auto file = dialog->get_file();
                if (file) {
                    try {
                        logo_pixbuf = Gdk::Pixbuf::create_from_file(file->get_path());
                        logo_enable.set_active(true);
                        update_qr();
                        show_info("Logo selected", file->get_path());
                    } catch (const Glib::FileError& e) {
                        show_error(std::string("File error: ") + e.what());
                    } catch (const Gdk::PixbufError& e) {
                        show_error(std::string("Pixbuf error: ") + e.what());
                    }
                }
            }
            delete dialog;
        });
        dialog->show();
    }

    // drawing callback (preview)
    void on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
        // reuse a drawing routine that keeps QR square and centers it
        draw_qr(cr, width, height);
    }

    void draw_qr(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
        int modules = qr.getSize();
        if (modules <= 0) return;

        // constants
        const int border_modules = 4;
        const double frame_thickness = std::max(8.0, std::min(width, height) * 0.03);
        const double label_height = std::max(28.0, std::min(width, height) * 0.08);

        double side = std::min(width, height) - 2.0 * frame_thickness - label_height;
        if (side <= 0) side = std::min(width, height) - 2.0 * frame_thickness;
        double pixelsPerModule = std::floor(side / (modules + 2*border_modules));
        if (pixelsPerModule < 1.0) pixelsPerModule = 1.0;

        double qrPixelSize = (modules + 2*border_modules) * pixelsPerModule;
        double outWidth = qrPixelSize + 2.0 * frame_thickness;
        double outHeight = outWidth + label_height;
        double offsetX = (width - outWidth) / 2.0;
        double offsetY = (height - outHeight) / 2.0;

        // clear
        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->paint();

        // frame
        cr->set_source_rgb(0.0, 0.0, 0.0);
        cr->rectangle(offsetX, offsetY, outWidth, outWidth);
        cr->fill();

        // inner
        double innerX = offsetX + frame_thickness;
        double innerY = offsetY + frame_thickness;
        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->rectangle(innerX, innerY, qrPixelSize, qrPixelSize);
        cr->fill();

        // draw modules
        Gdk::RGBA rgba = color_button.get_rgba();
        cr->set_source_rgb(rgba.get_red(), rgba.get_green(), rgba.get_blue());
        std::string shape = shape_combo.get_active_text();
        double startX = innerX + border_modules * pixelsPerModule;
        double startY = innerY + border_modules * pixelsPerModule;

        cr->set_antialias(Cairo::Antialias::ANTIALIAS_NONE);

        for (int y = 0; y < modules; ++y) {
            for (int x = 0; x < modules; ++x) {
                if (!qr.getModule(x, y)) continue;
                double rx = startX + x * pixelsPerModule;
                double ry = startY + y * pixelsPerModule;
                if (shape == "Square") {
                    cr->rectangle(rx, ry, pixelsPerModule, pixelsPerModule);
                    cr->fill();
                } else if (shape == "Circle") {
                    cr->arc(rx + pixelsPerModule/2.0, ry + pixelsPerModule/2.0, pixelsPerModule/2.0, 0.0, 2.0*M_PI);
                    cr->fill();
                } else { // Rounded
                    double r = std::max(1.0, pixelsPerModule * 0.25);
                    double x0 = rx, y0 = ry, x1 = rx + pixelsPerModule, y1 = ry + pixelsPerModule;
                    cr->begin_new_sub_path();
                    cr->arc(x1 - r, y0 + r, r, -M_PI/2.0, 0.0);
                    cr->arc(x1 - r, y1 - r, r, 0.0, M_PI/2.0);
                    cr->arc(x0 + r, y1 - r, r, M_PI/2.0, M_PI);
                    cr->arc(x0 + r, y0 + r, r, M_PI, 3.0*M_PI/2.0);
                    cr->close_path();
                    cr->fill();
                }
            }
        }

        // logo (proportional)
        if (logo_enable.get_active() && logo_pixbuf) {
            double pct = logo_size_adjustment->get_value() / 100.0;
            double logo_max = qrPixelSize * pct;
            int orig_w = logo_pixbuf->get_width();
            int orig_h = logo_pixbuf->get_height();
            if (orig_w > 0 && orig_h > 0) {
                double scale_logo = std::min(logo_max / orig_w, logo_max / orig_h);
                int new_w = std::max(1, int(orig_w * scale_logo + 0.5));
                int new_h = std::max(1, int(orig_h * scale_logo + 0.5));
                double lx = innerX + (qrPixelSize - new_w) / 2.0;
                double ly = innerY + (qrPixelSize - new_h) / 2.0;

                // white pad
                double pad = std::max(2.0, std::min(new_w, new_h) * 0.06);
                cr->set_source_rgb(1.0, 1.0, 1.0);
                cr->rectangle(lx - pad, ly - pad, new_w + 2*pad, new_h + 2*pad);
                cr->fill();

                // scale & paint via C API (robust)
                auto scaled = logo_pixbuf->scale_simple(new_w, new_h, Gdk::InterpType::BILINEAR);
                if (scaled) {
                    cairo_t* c = cr->cobj();
                    gdk_cairo_set_source_pixbuf(c, scaled->gobj(), lx, ly);
                    cairo_paint(c);
                }
            }
        }

        // label bar bottom
        double labelX = offsetX;
        double labelY = offsetY + outWidth;
        cr->set_source_rgb(0.0, 0.0, 0.0);
        cr->rectangle(labelX, labelY, outWidth, label_height);
        cr->fill();

        // "SCAN ME" centered white
        cairo_t* c = cr->cobj();
        if (c) {
            cairo_select_font_face(c, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            double fsize = std::max(12.0, label_height * 0.45);
            cairo_set_font_size(c, fsize);
            const char* txt = "SCAN ME";
            cairo_text_extents_t te;
            cairo_text_extents(c, txt, &te);
            double tx = offsetX + (outWidth - te.width) / 2.0 - te.x_bearing;
            double ty = labelY + (label_height - te.height) / 2.0 - te.y_bearing;
            cairo_set_source_rgb(c, 1.0, 1.0, 1.0);
            cairo_move_to(c, tx, ty);
            cairo_show_text(c, txt);
        }
    }

    // --- save PNG ---
    void on_save_png() {
        // ensure we have content
        if (qr.getSize() <= 0) {
            show_error("No QR to save.");
            return;
        }

        auto *dialog = new Gtk::FileChooserDialog("Save QR Code as PNG", Gtk::FileChooser::Action::SAVE);
        dialog->set_transient_for(*this);
        dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
        dialog->add_button("_Save", Gtk::ResponseType::OK);

        auto filter = Gtk::FileFilter::create();
        filter->set_name("PNG images");
        filter->add_pattern("*.png");
        dialog->add_filter(filter);

        dialog->signal_response().connect([this, dialog](int response){
            if (response == Gtk::ResponseType::OK) {
                auto file = dialog->get_file();
                if (file) {
                    std::string path = file->get_path();
                    if (path.size() < 4 || path.substr(path.size()-4) != ".png") path += ".png";
                    try {
                        write_png_with_cairo(path);
                        show_info("Saved", "QR image saved successfully.");
                    } catch (const std::exception &e) {
                        show_error(std::string("Failed to save PNG: ") + e.what());
                    }
                }
            }
            delete dialog;
        });
        dialog->show();
    }

    void write_png_with_cairo(const std::string &filename) {
        int modules = qr.getSize();
        if (modules <= 0) throw std::runtime_error("No QR code generated.");

        const int border_modules = 4;
        const int frame_thickness = 20;
        const int label_height = 60;
        const int module_pixel = 10;

        int qrPixelSize = (modules + 2*border_modules) * module_pixel;
        int outWidth = qrPixelSize + 2*frame_thickness;
        int outHeight = outWidth + label_height;

        cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, outWidth, outHeight);
        if (!surf) throw std::runtime_error("Failed to create surface");
        cairo_t* cr = cairo_create(surf);
        if (!cr) { cairo_surface_destroy(surf); throw std::runtime_error("Failed to create context"); }

        cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
        cairo_set_source_rgb(cr, 1,1,1); cairo_paint(cr);

        // frame
        cairo_set_source_rgb(cr, 0,0,0);
        cairo_rectangle(cr, 0, 0, outWidth, outWidth);
        cairo_fill(cr);

        // inner white
        cairo_set_source_rgb(cr, 1,1,1);
        cairo_rectangle(cr, frame_thickness, frame_thickness, qrPixelSize, qrPixelSize);
        cairo_fill(cr);

        // modules color/shape
        Gdk::RGBA rgba = color_button.get_rgba();
        cairo_set_source_rgb(cr, rgba.get_red(), rgba.get_green(), rgba.get_blue());
        std::string shape = shape_combo.get_active_text();
        int startX = frame_thickness + border_modules * module_pixel;
        int startY = frame_thickness + border_modules * module_pixel;

        for (int y = 0; y < modules; ++y) {
            for (int x = 0; x < modules; ++x) {
                if (!qr.getModule(x, y)) continue;
                int rx = startX + x * module_pixel;
                int ry = startY + y * module_pixel;
                if (shape == "Square") {
                    cairo_rectangle(cr, rx, ry, module_pixel, module_pixel);
                    cairo_fill(cr);
                } else if (shape == "Circle") {
                    cairo_arc(cr, rx + module_pixel/2.0, ry + module_pixel/2.0, module_pixel/2.0, 0, 2*M_PI);
                    cairo_fill(cr);
                } else {
                    double r = std::max(1.0, module_pixel * 0.25);
                    double x0 = rx, y0 = ry, x1 = rx + module_pixel, y1 = ry + module_pixel;
                    cairo_new_sub_path(cr);
                    cairo_arc(cr, x1 - r, y0 + r, r, -M_PI/2.0, 0.0);
                    cairo_arc(cr, x1 - r, y1 - r, r, 0.0, M_PI/2.0);
                    cairo_arc(cr, x0 + r, y1 - r, r, M_PI/2.0, M_PI);
                    cairo_arc(cr, x0 + r, y0 + r, r, M_PI, 3.0*M_PI/2.0);
                    cairo_close_path(cr);
                    cairo_fill(cr);
                }
            }
        }

        // logo (proportional)
        if (logo_enable.get_active() && logo_pixbuf) {
            double logo_percent = logo_size_adjustment->get_value() / 100.0;
            int logo_max_side = std::max(1, int(qrPixelSize * logo_percent + 0.5));
            int orig_w = logo_pixbuf->get_width();
            int orig_h = logo_pixbuf->get_height();
            if (orig_w > 0 && orig_h > 0) {
                double scale = std::min((double)logo_max_side / orig_w, (double)logo_max_side / orig_h);
                int new_w = std::max(1, int(orig_w * scale + 0.5));
                int new_h = std::max(1, int(orig_h * scale + 0.5));
                double lx = frame_thickness + (qrPixelSize - new_w) / 2.0;
                double ly = frame_thickness + (qrPixelSize - new_h) / 2.0;

                double pad = std::max(2.0, std::min(new_w, new_h) * 0.06);
                cairo_set_source_rgb(cr, 1,1,1);
                cairo_rectangle(cr, lx - pad, ly - pad, new_w + 2*pad, new_h + 2*pad);
                cairo_fill(cr);

                auto scaled = logo_pixbuf->scale_simple(new_w, new_h, Gdk::InterpType::BILINEAR);
                if (scaled) {
                    gdk_cairo_set_source_pixbuf(cr, scaled->gobj(), lx, ly);
                    cairo_paint(cr);
                }
            }
        }

        // label bar
        cairo_set_source_rgb(cr, 0,0,0);
        cairo_rectangle(cr, 0, outWidth, outWidth, label_height);
        cairo_fill(cr);

        // "SCAN ME"
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        double font_size = std::max(12.0, label_height * 0.45);
        cairo_set_font_size(cr, font_size);
        const char* text = "SCAN ME";
        cairo_text_extents_t te;
        cairo_text_extents(cr, text, &te);
        double tx = (outWidth - te.width) / 2.0 - te.x_bearing;
        double ty = outWidth + (label_height - te.height) / 2.0 - te.y_bearing;
        cairo_set_source_rgb(cr, 1,1,1);
        cairo_move_to(cr, tx, ty);
        cairo_show_text(cr, text);

        // write and cleanup
        cairo_surface_write_to_png(surf, filename.c_str());
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    }

    // dialogs
    void show_error(const std::string &msg) {
        auto *d = new Gtk::MessageDialog(*this, "Error", false, Gtk::MessageType::ERROR);
        d->set_secondary_text(msg);
        d->add_button("OK", Gtk::ResponseType::OK);
        d->signal_response().connect([d](int){ delete d; });
        d->show();
    }
    void show_info(const std::string &title, const std::string &msg) {
        auto *d = new Gtk::MessageDialog(*this, title, false, Gtk::MessageType::INFO);
        d->set_secondary_text(msg);
        d->add_button("OK", Gtk::ResponseType::OK);
        d->signal_response().connect([d](int){ delete d; });
        d->show();
    }
};

int main(int argc, char *argv[]) {
    auto app = Gtk::Application::create("org.example.qrgtkmm");
    return app->make_window_and_run<QRWindow>(argc, argv);
}
