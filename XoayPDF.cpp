// -*- coding: utf-8 -*-
// XoayPDF.cpp — Xoay trang PDF đúng hướng chữ, bản C++ siêu nhẹ cho Windows 10/11
//
// Nguyên lý: dùng engine có sẵn của Windows, không nhúng thư viện OCR nào:
//   - Windows.Data.Pdf   : render trang PDF thành ảnh
//   - Windows.Media.Ocr  : OCR thử 4 hướng (0/90/180/270), hướng nào đọc được
//                          nhiều chữ nhất là hướng đúng
//   - qpdf               : ghi cờ /Rotate vào file PDF (lossless, không render lại)
//
// Cách dùng: kéo file PDF (hoặc gõ đường dẫn) thả vào XoayPDF.exe
//   XoayPDF.exe file1.pdf file2.pdf "D:\thu muc"
// Kết quả: <tên gốc>_xoay.pdf cạnh file gốc. File gốc không bị ghi đè.

#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Globalization.h>

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Data::Pdf;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Ocr;

namespace fs = std::filesystem;

// ------------------ CẤU HÌNH ------------------
static const int      MIN_CHARS  = 20;    // trang đọc được ít hơn ~20 ký tự -> bỏ qua (bản vẽ ít chữ)
static const double   RATIO      = 1.30;  // hướng mới phải đọc được nhiều chữ hơn hướng hiện tại >= 30%
static const uint32_t RENDER_MAX = 1400;  // độ phân giải render (px, cạnh dài nhất)
// -----------------------------------------------

static std::string utf8(std::wstring const& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

// Đếm tổng số ký tự OCR đọc được trên ảnh
static int ocr_char_count(OcrEngine const& eng, SoftwareBitmap const& bmp) {
    try {
        OcrResult res = eng.RecognizeAsync(bmp).get();
        int total = 0;
        for (auto const& line : res.Lines())
            for (auto const& word : line.Words())
                total += (int)word.Text().size();
        return total;
    } catch (...) {
        return 0;
    }
}

// Giải mã ảnh từ decoder kèm phép xoay
static SoftwareBitmap decode_rotated(BitmapDecoder const& dec, BitmapRotation rot) {
    BitmapTransform t;
    t.Rotation(rot);
    return dec.GetSoftwareBitmapAsync(
        BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied, t,
        ExifOrientationMode::IgnoreExifOrientation,
        ColorManagementMode::DoNotColorManage).get();
}

static int process_pdf(fs::path const& in, OcrEngine const& eng) {
    printf("\n=== %s ===\n", utf8(in.filename().wstring()).c_str());

    // 1) Mở bằng WinRT để render
    PdfDocument pdf{ nullptr };
    try {
        auto file = StorageFile::GetFileFromPathAsync(hstring(fs::absolute(in).wstring())).get();
        pdf = PdfDocument::LoadFromFileAsync(file).get();
    } catch (...) {
        printf("  [LOI] Khong mo duoc (file hong hoac co mat khau).\n");
        return 0;
    }

    // 2) Mở bằng qpdf để sửa cờ xoay
    QPDF q;
    try {
        q.processFile(utf8(fs::absolute(in).wstring()).c_str());
    } catch (std::exception const& e) {
        printf("  [LOI] qpdf: %s\n", e.what());
        return 0;
    }
    auto pages = QPDFPageDocumentHelper(q).getAllPages();
    uint32_t n = (std::min)((uint32_t)pages.size(), pdf.PageCount());

    static const BitmapRotation ROTS[4] = {
        BitmapRotation::None, BitmapRotation::Clockwise90Degrees,
        BitmapRotation::Clockwise180Degrees, BitmapRotation::Clockwise270Degrees };

    int changed = 0;
    for (uint32_t i = 0; i < n; ++i) {
        int counts[4] = { 0, 0, 0, 0 };
        try {
            PdfPage page = pdf.GetPage(i);
            auto sz = page.Size();
            double longest = (sz.Width > sz.Height) ? sz.Width : sz.Height;
            double scale = (double)RENDER_MAX / longest;
            if (scale > 3.0) scale = 3.0;

            PdfPageRenderOptions opt;
            opt.DestinationWidth((uint32_t)(sz.Width * scale));
            opt.DestinationHeight((uint32_t)(sz.Height * scale));

            InMemoryRandomAccessStream ms;
            page.RenderToStreamAsync(ms, opt).get();
            page.Close();
            ms.Seek(0);
            BitmapDecoder dec = BitmapDecoder::CreateAsync(ms).get();

            for (int k = 0; k < 4; ++k) {
                SoftwareBitmap bmp = decode_rotated(dec, ROTS[k]);
                counts[k] = ocr_char_count(eng, bmp);
                bmp.Close();
            }
        } catch (...) {
            printf("  Trang %3u: [LOI render/OCR], bo qua\n", i + 1);
            continue;
        }

        int best = 0;
        for (int k = 1; k < 4; ++k) if (counts[k] > counts[best]) best = k;

        if (counts[best] < MIN_CHARS) {
            printf("  Trang %3u: it chu (%d ky tu), bo qua\n", i + 1, counts[best]);
        } else if (best == 0) {
            printf("  Trang %3u: da dung huong (%d ky tu)\n", i + 1, counts[0]);
        } else if ((double)counts[best] < RATIO * (double)(std::max)(counts[0], 1)) {
            printf("  Trang %3u: nghi lech %d do nhung khong chac (%d vs %d ky tu), bo qua\n",
                   i + 1, best * 90, counts[best], counts[0]);
        } else {
            pages[i].rotatePage(best * 90, true);  // xoay tuong doi, lossless
            ++changed;
            printf("  Trang %3u: XOAY %d do (%d vs %d ky tu)\n",
                   i + 1, best * 90, counts[best], counts[0]);
        }
    }
    pdf = nullptr;

    if (changed == 0) {
        printf("  -> Khong trang nao can xoay.\n");
        return 0;
    }
    fs::path out = in.parent_path() / (in.stem().wstring() + L"_xoay" + in.extension().wstring());
    try {
        QPDFWriter w(q, utf8(fs::absolute(out).wstring()).c_str());
        w.write();
        printf("  -> Da xoay %d trang. Luu: %s\n", changed, utf8(out.wstring()).c_str());
    } catch (std::exception const& e) {
        printf("  [LOI] Khong ghi duoc file: %s\n", e.what());
        return 0;
    }
    return changed;
}

static void collect(fs::path const& p, std::vector<fs::path>& out) {
    std::error_code ec;
    if (fs::is_directory(p, ec)) {
        for (auto const& e : fs::directory_iterator(p, ec)) {
            auto ext = e.path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
            if (ext == L".pdf") out.push_back(e.path());
        }
    } else if (fs::exists(p, ec)) {
        out.push_back(p);
    } else {
        printf("[BO QUA] Khong ton tai: %s\n", utf8(p.wstring()).c_str());
    }
}

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    init_apartment(apartment_type::multi_threaded);

    printf("XoayPDF - Xoay trang PDF dung huong chu (Windows OCR, lossless)\n");

    if (argc < 2) {
        printf("\nCach dung: keo file PDF (hoac thu muc) tha vao XoayPDF.exe\n"
               "Hoac:      XoayPDF.exe file1.pdf \"D:\\thu muc\"\n\nNhan Enter de thoat...");
        (void)getchar();
        return 0;
    }

    // Tạo OCR engine từ ngôn ngữ người dùng; fallback tiếng Anh, rồi tiếng Việt
    OcrEngine eng = OcrEngine::TryCreateFromUserProfileLanguages();
    if (!eng) eng = OcrEngine::TryCreateFromLanguage(winrt::Windows::Globalization::Language(L"en-US"));
    if (!eng) eng = OcrEngine::TryCreateFromLanguage(winrt::Windows::Globalization::Language(L"vi"));
    if (!eng) {
        printf("[LOI] Windows chua co goi ngon ngu OCR nao.\n"
               "Vao Settings > Time & Language > Language & region > Add a language,\n"
               "them English hoac Vietnamese roi chay lai.\nNhan Enter de thoat...");
        (void)getchar();
        return 1;
    }
    printf("OCR engine: %s\n", utf8(std::wstring(eng.RecognizerLanguage().DisplayName())).c_str());

    std::vector<fs::path> pdfs;
    for (int i = 1; i < argc; ++i) collect(argv[i], pdfs);

    int total = 0;
    for (auto const& p : pdfs) total += process_pdf(p, eng);

    printf("\nXONG. Tong cong xoay %d trang / %zu file.\nNhan Enter de thoat...", total, pdfs.size());
    (void)getchar();
    return 0;
}
