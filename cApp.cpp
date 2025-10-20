#include <wx/wx.h>
#include "cMain.h"

class cApp : public wxApp {
public:
    virtual bool OnInit() {
        // 注册 PNG/JPEG/GIF 图片处理器
        wxInitAllImageHandlers();

        cMain* frame = new cMain();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(cApp);
