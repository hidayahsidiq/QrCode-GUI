// main.cpp (perbaikan: gunakan ANTIALIAS_NONE)
#include <gtkmm.h>
#include <cairomm/context.h>
#include "QrCode.hpp"
#include "TinyPngOut.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <cmath>

using qrcodegen::QrCode;

class QRWindow : public Gtk::Window {
public:
    QRWindow() {
        set_title("QR Code Generator - GTKmm 4");
        set_default_size(560, 720);

        vbox.set_margin(10);
        vbox.set_spacing(8);
        set_child(vbox);

        // Mode selector
        mode_combo.append("Text / URL");
        mode_combo.append("Wi-Fi");
        mode_combo.append("vCard");
        mode_combo.append("Image URL");
        mode_combo.set_active(0);
        mode_combo.signal_changed().connect(sigc::mem_fun(*this, &QRWindow::on_mode_changed));
        vbox.append(mode_combo);

        // Generic text entry (Text / Image URL)
        entry.set_placeholder_text("Enter text or URL...");
        vbox.append(entry);

        // Wi-Fi inputs
        wifi_box.set_orientation(Gtk::Orientation::VERTICAL);
        wifi_ssid.set_placeholder_text("SSID (network name)");
        wifi_pass.set_placeholder_text("Password");
        wifi_enc.append("WPA/WPA2"); wifi_enc.append("WEP"); wifi_enc.append("None");
        wifi_enc.set_active_text("WPA/WPA2");
        wifi_box.append(wifi_ssid);
        wifi_box.append(wifi_pass);
        wifi_box.append(wifi_enc);
        wifi_box.set_visible(false);
        vbox.append(wifi_box);

        // vCard inputs
        vcard_box.set_orientation(Gtk::Orientation::VERTICAL);
        vcard_name.set_placeholder_text("Full name (FN)");
        vcard_tel.set_placeholder_text("Phone");
        vcard_email.set_placeholder_text("Email");
        vcard_box.append(vcard_name);
        vcard_box.append(vcard_tel);
        vcard_box.append(vcard_email);
        vcard_box.set_visible(false);
        vbox.append(vcard_box);

        // Buttons
        btn_generate.set_label("Generate QR");
        btn_generate.signal_clicked().connect(sigc::mem_fun(*this, &QRWindow::on_generate));
        vbox.append(btn_generate);

        btn_save.set_label("Save as PNG");
        btn_save.signal_clicked().connect(sigc::mem_fun(*this, &QRWindow::on_save_png));
        vbox.append(btn_save);

        // Drawing area
        drawing.set_content_width(520);
        drawing.set_content_height(520);
        drawing.set_draw_func(sigc::mem_fun(*this, &QRWindow::on_draw));
        vbox.append(drawing);

        // Initialize with a sample QR
        last_content = "https://raymii.org";
        qr = QrCode::encodeText(last_content.c_str(), QrCode::Ecc::MEDIUM);
    }

private:
    // UI
    Gtk::Box vbox{Gtk::Orientation::VERTICAL};
    Gtk::ComboBoxText mode_combo;
    Gtk::Entry entry;

    Gtk::Box wifi_box{Gtk::Orientation::VERTICAL};
    Gtk::Entry wifi_ssid;
    Gtk::Entry wifi_pass;
    Gtk::ComboBoxText wifi_enc; // "WPA/WPA2", "WEP", "None"

    Gtk::Box vcard_box{Gtk::Orientation::VERTICAL};
    Gtk::Entry vcard_name;
    Gtk::Entry vcard_tel;
    Gtk::Entry vcard_email;

    Gtk::Button btn_generate, btn_save;
    Gtk::DrawingArea drawing;

    // QR state
    QrCode qr = QrCode::encodeText(" ", QrCode::Ecc::LOW);
    std::string last_content;

    // --- UI callbacks ---
    void on_mode_changed() {
        std::string mode = mode_combo.get_active_text();

        // Hide all optional groups by default
        wifi_box.set_visible(false);
        vcard_box.set_visible(false);
        entry.set_visible(false);

        if (mode == "Text / URL") {
            entry.set_visible(true);
        } else if (mode == "Wi-Fi") {
            wifi_box.set_visible(true);
        } else if (mode == "vCard") {
            vcard_box.set_visible(true);
        } else if (mode == "Image URL") {
            entry.set_visible(true);
        }
    }

    void on_generate() {
        try {
            last_content = build_content_from_inputs();
        } catch (const std::exception &e) {
            show_error(e.what());
            return;
        }

        // Encode using Nayuki QrCode class
        try {
            qr = QrCode::encodeText(last_content.c_str(), QrCode::Ecc::MEDIUM);
        } catch (const std::exception &e) {
            show_error(std::string("Failed to encode QR: ") + e.what());
            return;
        }

        drawing.queue_draw();
    }

    std::string build_content_from_inputs() {
        std::string mode = mode_combo.get_active_text();
        if (mode == "Text / URL") {
            std::string t = entry.get_text();
            if (t.empty()) throw std::runtime_error("Please enter text or URL.");
            return t;
        }
        else if (mode == "Wi-Fi") {
            std::string ssid = wifi_ssid.get_text();
            std::string pass = wifi_pass.get_text();
            std::string enc = wifi_enc.get_active_text();
            if (ssid.empty()) throw std::runtime_error("Please enter Wi-Fi SSID.");
            std::string enc_code = "WPA";
            if (enc == "WEP") enc_code = "WEP";
            else if (enc == "None") enc_code = "nopass";
            // build according to Wi-Fi QR code standard
            std::ostringstream ss;
            if (enc_code == "nopass") {
                ss << "WIFI:T:nopass;S:" << escape_semicolons(ssid) << ";;";
            } else {
                ss << "WIFI:T:" << enc_code << ";S:" << escape_semicolons(ssid)
                   << ";P:" << escape_semicolons(pass) << ";;";
            }
            return ss.str();
        }
        else if (mode == "vCard") {
            std::string name = vcard_name.get_text();
            if (name.empty()) throw std::runtime_error("Please enter vCard full name.");
            std::string tel = vcard_tel.get_text();
            std::string email = vcard_email.get_text();
            std::ostringstream ss;
            ss << "BEGIN:VCARD\nVERSION:3.0\nFN:" << escape_vcard(name) << "\n";
            if (!tel.empty()) ss << "TEL:" << escape_vcard(tel) << "\n";
            if (!email.empty()) ss << "EMAIL:" << escape_vcard(email) << "\n";
            ss << "END:VCARD";
            return ss.str();
        }
        else if (mode == "Image URL") {
            std::string t = entry.get_text();
            if (t.empty()) throw std::runtime_error("Please enter image URL.");
            return t;
        }
        throw std::runtime_error("Unknown mode selected.");
    }

    // helper: escape semicolons and backslashes for WiFi string
    static std::string escape_semicolons(const std::string &s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '\\' || c == ';' || c == ',') { out.push_back('\\'); out.push_back(c); }
            else out.push_back(c);
        }
        return out;
    }

    // helper for vCard basic escaping (very simple)
    static std::string escape_vcard(const std::string &s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '\n') out += "\\n";
            else out.push_back(c);
        }
        return out;
    }

    // --- Draw QR to Cairo ---
    void on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
        int msize = qr.getSize();
        if (msize <= 0) return;

        // Turn off antialiasing for crisp square modules
        cr->set_antialias(Cairo::Antialias::ANTIALIAS_NONE);

        // White background
        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->paint();

        double scale = std::min((double)width / msize, (double)height / msize);

        cr->set_source_rgb(0.0, 0.0, 0.0);

        // Draw rectangles for modules; accumulate then fill once
        for (int y = 0; y < msize; ++y) {
            for (int x = 0; x < msize; ++x) {
                if (qr.getModule(x, y)) {
                    double rx = std::floor(x * scale);
                    double ry = std::floor(y * scale);
                    double rw = std::ceil((x + 1) * scale) - rx;
                    double rh = std::ceil((y + 1) * scale) - ry;
                    cr->rectangle(rx, ry, rw, rh);
                }
            }
        }
        cr->fill();
    }

    // --- Save PNG (with file chooser) ---
    void on_save_png() {
        // Ensure there's a generated QR; if not, try to generate from current inputs
        if (qr.getSize() <= 0) {
            try { last_content = build_content_from_inputs(); qr = QrCode::encodeText(last_content.c_str(), QrCode::Ecc::MEDIUM); }
            catch (const std::exception &e) { show_error(e.what()); return; }
        }

        auto dialog = new Gtk::FileChooserDialog(*this, "Save QR Code as PNG", Gtk::FileChooser::Action::SAVE);
        dialog->set_modal(true);

        // Filter .png
        auto filter_png = Gtk::FileFilter::create();
        filter_png->set_name("PNG images");
        filter_png->add_pattern("*.png");
        dialog->add_filter(filter_png);

        dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
        dialog->add_button("_Save", Gtk::ResponseType::OK);

        dialog->signal_response().connect([this, dialog](int response) {
            if (response == Gtk::ResponseType::OK) {
                auto file = dialog->get_file();
                if (file) {
                    std::string path = file->get_path();
                    try {
                        save_png_file(path);
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

    // Write PNG using TinyPngOut, row-by-row
    void save_png_file(std::string filename_in) {
        int msize = qr.getSize();
        if (msize <= 0) throw std::runtime_error("No QR code generated.");

        const int module_pixel = 10;   // pixels per module (bigger -> crisper)
        const int border_modules = 4; // white border (modules)
        const int outSize = (msize + border_modules * 2) * module_pixel;

        // Ensure .png extension
        std::string filename = filename_in;
        if (filename.size() < 4 || filename.substr(filename.size() - 4) != ".png")
            filename += ".png";

        std::ofstream of(filename, std::ios::binary);
        if (!of) throw std::runtime_error("Cannot open file for writing: " + filename);

        TinyPngOut png(outSize, outSize, of);

        std::vector<uint8_t> row(outSize * 3);

        // For each output row, fill RGB bytes
        for (int y = 0; y < outSize; ++y) {
            std::fill(row.begin(), row.end(), 0xFF); // white background

            int qrY = y / module_pixel - border_modules;
            for (int x = 0; x < outSize; ++x) {
                int qrX = x / module_pixel - border_modules;
                if (qrX >= 0 && qrX < msize && qrY >= 0 && qrY < msize) {
                    if (qr.getModule(qrX, qrY)) {
                        int idx = x * 3;
                        row[idx] = 0;
                        row[idx + 1] = 0;
                        row[idx + 2] = 0;
                    }
                }
            }
            // Write one row at a time
            png.write(row.data(), row.size());
        }
    }

    // --- Simple message dialogs (GTKmm4 style) ---
    void show_error(const std::string &msg) {
        auto d = new Gtk::MessageDialog(*this, "Error", false, Gtk::MessageType::ERROR);
        d->set_secondary_text(msg);
        d->set_modal(true);
        d->add_button("OK", Gtk::ResponseType::OK);
        d->signal_response().connect([d](int){ delete d; });
        d->show();
    }

    void show_info(const std::string &title, const std::string &msg) {
        auto d = new Gtk::MessageDialog(*this, title, false, Gtk::MessageType::INFO);
        d->set_secondary_text(msg);
        d->set_modal(true);
        d->add_button("OK", Gtk::ResponseType::OK);
        d->signal_response().connect([d](int){ delete d; });
        d->show();
    }
};

int main(int argc, char *argv[]) {
    auto app = Gtk::Application::create("org.example.qrgtkmm");
    return app->make_window_and_run<QRWindow>(argc, argv);
}
