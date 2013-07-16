;Language: Vietnamese (1066)
;By NGUY��N Ma�nh Hu�ng <loveleeyoungae@yahoo.com>

!insertmacro LANGFILE "Vietnamese" "Vietnamese"

!ifdef MUI_WELCOMEPAGE
  ${LangFileString} MUI_TEXT_WELCOME_INFO_TITLE "Cha�o m��ng ���n v��i Tr�� li� Ca�i ���t $(^NameDA)"
  ${LangFileString} MUI_TEXT_WELCOME_INFO_TEXT "Tri�nh tr�� li� se� h���ng d��n ba�n vi��c ca�i ���t $(^NameDA).$\r$\n$\r$\nBa�n n�n �o�ng t��t ca� ca�c ch��ng tri�nh kha�c tr���c khi b��t ���u ca�i ���t. �i��u na�y co� th�� giu�p c��p nh��t ca�c t��p tin h�� th��ng ma� kh�ng c��n pha�i kh��i ���ng la�i ma�y ti�nh.$\r$\n$\r$\n$_CLICK"
!endif

!ifdef MUI_UNWELCOMEPAGE
  ${LangFileString} MUI_UNTEXT_WELCOME_INFO_TITLE "Cha�o m��ng ���n v��i Tr�� li� G�� bo� $(^NameDA)"
  ${LangFileString} MUI_UNTEXT_WELCOME_INFO_TEXT "Tri�nh tr�� li� se� h���ng d��n ba�n vi��c g�� bo� $(^NameDA).$\r$\n$\r$\nTr���c khi b��t ���u g�� bo�, ha�y ch��c ch��n r��ng $(^NameDA) �ang kh�ng cha�y.$\r$\n$\r$\n$_CLICK"
!endif

!ifdef MUI_LICENSEPAGE
  ${LangFileString} MUI_TEXT_LICENSE_TITLE "Tho�a thu��n Gi��y phe�p"
  ${LangFileString} MUI_TEXT_LICENSE_SUBTITLE "Vui lo�ng xem xe�t ca�c �i��u khoa�n gi��y phe�p tr���c khi ca�i ���t $(^NameDA)."
  ${LangFileString} MUI_INNERTEXT_LICENSE_BOTTOM "N��u ba�n ch��p thu��n ca�c �i��u khoa�n cu�a tho�a thu��n, ha�y nh��n �T�i ���ng y� ��� ti��p tu�c. Ba�n pha�i ch��p thu��n ba�n tho�a thu��n ��� ca�i ���t $(^NameDA)."
  ${LangFileString} MUI_INNERTEXT_LICENSE_BOTTOM_CHECKBOX "N��u ba�n ch��p thu��n ca�c �i��u khoa�n cu�a tho�a thu��n, ha�y nh��n � b�n d���i. Ba�n pha�i ch��p thu��n ba�n tho�a thu��n ��� ca�i ���t $(^NameDA). $_CLICK"
  ${LangFileString} MUI_INNERTEXT_LICENSE_BOTTOM_RADIOBUTTONS "N��u ba�n ch��p thu��n ca�c �i��u khoa�n cu�a tho�a thu��n, ha�y cho�n � ���u ti�n b�n d���i. Ba�n pha�i ch��p thu��n ba�n tho�a thu��n ��� ca�i ���t $(^NameDA). $_CLICK"
!endif

!ifdef MUI_UNLICENSEPAGE
  ${LangFileString} MUI_UNTEXT_LICENSE_TITLE "Tho�a thu��n Gi��y phe�p"
  ${LangFileString} MUI_UNTEXT_LICENSE_SUBTITLE "Vui lo�ng xem xe�t ca�c �i��u khoa�n gi��y phe�p tr���c khi g�� bo� $(^NameDA)."
  ${LangFileString} MUI_UNINNERTEXT_LICENSE_BOTTOM "N��u ba�n ch��p thu��n ca�c �i��u khoa�n cu�a tho�a thu��n, ha�y nh��n �T�i ���ng y� ��� ti��p tu�c. Ba�n pha�i ch��p thu��n ba�n tho�a thu��n ��� g�� bo� $(^NameDA)."
  ${LangFileString} MUI_UNINNERTEXT_LICENSE_BOTTOM_CHECKBOX "N��u ba�n ch��p thu��n ca�c �i��u khoa�n cu�a tho�a thu��n, ha�y nh��n � b�n d���i. Ba�n pha�i ch��p thu��n ba�n tho�a thu��n ��� g�� bo� $(^NameDA). $_CLICK"
  ${LangFileString} MUI_UNINNERTEXT_LICENSE_BOTTOM_RADIOBUTTONS "N��u ba�n ch��p thu��n ca�c �i��u khoa�n cu�a tho�a thu��n, ha�y cho�n � ���u ti�n b�n d���i. Ba�n pha�i ch��p thu��n ba�n tho�a thu��n ��� g�� bo� $(^NameDA). $_CLICK"
!endif

!ifdef MUI_LICENSEPAGE | MUI_UNLICENSEPAGE
  ${LangFileString} MUI_INNERTEXT_LICENSE_TOP "��n Page Down ��� xem ph��n co�n la�i cu�a tho�a thu��n."
!endif

!ifdef MUI_COMPONENTSPAGE
  ${LangFileString} MUI_TEXT_COMPONENTS_TITLE "Cho�n tha�nh ph��n"
  ${LangFileString} MUI_TEXT_COMPONENTS_SUBTITLE "Cho�n ca�c ti�nh n�ng cu�a $(^NameDA) ma� ba�n mu��n ca�i ���t."
!endif

!ifdef MUI_UNCOMPONENTSPAGE
  ${LangFileString} MUI_UNTEXT_COMPONENTS_TITLE "Cho�n tha�nh ph��n"
  ${LangFileString} MUI_UNTEXT_COMPONENTS_SUBTITLE "Cho�n ca�c ti�nh n�ng cu�a $(^NameDA) ma� ba�n mu��n g�� bo�."
!endif

!ifdef MUI_COMPONENTSPAGE | MUI_UNCOMPONENTSPAGE
  ${LangFileString} MUI_INNERTEXT_COMPONENTS_DESCRIPTION_TITLE "M� ta�"
  !ifndef NSIS_CONFIG_COMPONENTPAGE_ALTERNATIVE
    ${LangFileString} MUI_INNERTEXT_COMPONENTS_DESCRIPTION_INFO "R� chu��t l�n tr�n m��t tha�nh ph��n ��� th��y m� ta� cu�a no�."
  !else
    ${LangFileString} MUI_INNERTEXT_COMPONENTS_DESCRIPTION_INFO "R� chu��t l�n tr�n m��t tha�nh ph��n ��� th��y m� ta� cu�a no�."
  !endif
!endif

!ifdef MUI_DIRECTORYPAGE
  ${LangFileString} MUI_TEXT_DIRECTORY_TITLE "Cho�n th� mu�c ca�i ���t"
  ${LangFileString} MUI_TEXT_DIRECTORY_SUBTITLE "Cho�n th� mu�c ��� ca�i ���t $(^NameDA)."
!endif

!ifdef MUI_UNDIRECTORYPAGE
  ${LangFileString} MUI_UNTEXT_DIRECTORY_TITLE "Cho�n th� mu�c g�� bo�"
  ${LangFileString} MUI_UNTEXT_DIRECTORY_SUBTITLE "Cho�n th� mu�c ��� g�� bo� $(^NameDA)."
!endif

!ifdef MUI_INSTFILESPAGE
  ${LangFileString} MUI_TEXT_INSTALLING_TITLE "Ca�i ���t"
  ${LangFileString} MUI_TEXT_INSTALLING_SUBTITLE "Vui lo�ng ���i trong khi $(^NameDA) �ang ����c ca�i ���t."
  ${LangFileString} MUI_TEXT_FINISH_TITLE "Ca�i ���t hoa�n t��t"
  ${LangFileString} MUI_TEXT_FINISH_SUBTITLE "Vi��c ca�i ���t �a� hoa�n t��t tha�nh c�ng."
  ${LangFileString} MUI_TEXT_ABORT_TITLE "Ca�i ���t bi� hu�y"
  ${LangFileString} MUI_TEXT_ABORT_SUBTITLE "Vi��c ca�i ���t kh�ng hoa�n t��t tha�nh c�ng."
!endif

!ifdef MUI_UNINSTFILESPAGE
  ${LangFileString} MUI_UNTEXT_UNINSTALLING_TITLE "G�� bo�"
  ${LangFileString} MUI_UNTEXT_UNINSTALLING_SUBTITLE "Vui lo�ng ���i trong khi $(^NameDA) �ang ����c g�� bo�."
  ${LangFileString} MUI_UNTEXT_FINISH_TITLE "G�� bo� hoa�n t��t"
  ${LangFileString} MUI_UNTEXT_FINISH_SUBTITLE "Vi��c g�� bo� �a� hoa�n t��t tha�nh c�ng."
  ${LangFileString} MUI_UNTEXT_ABORT_TITLE "G�� bo� bi� hu�y"
  ${LangFileString} MUI_UNTEXT_ABORT_SUBTITLE "Vi��c g�� bo� kh�ng hoa�n t��t tha�nh c�ng."
!endif

!ifdef MUI_FINISHPAGE
  ${LangFileString} MUI_TEXT_FINISH_INFO_TITLE "Hoa�n t��t Ca�i ���t $(^NameDA)"
  ${LangFileString} MUI_TEXT_FINISH_INFO_TEXT "$(^NameDA) �a� ����c ca�i ���t va�o ma�y ti�nh cu�a ba�n.$\r$\n$\r$\nNh��n �Hoa�n tha�nh� ��� �o�ng Tri�nh tr�� li�."
  ${LangFileString} MUI_TEXT_FINISH_INFO_REBOOT "Ma�y ti�nh cu�a ba�n pha�i ����c kh��i ���ng la�i ��� hoa�n t��t vi��c ca�i ���t $(^NameDA). Ba�n co� mu��n kh��i ���ng la�i ngay kh�ng?"
!endif

!ifdef MUI_UNFINISHPAGE
  ${LangFileString} MUI_UNTEXT_FINISH_INFO_TITLE "Hoa�n t��t G�� bo� $(^NameDA)"
  ${LangFileString} MUI_UNTEXT_FINISH_INFO_TEXT "$(^NameDA) �a� ����c g�� bo� kho�i ma�y ti�nh cu�a ba�n.$\r$\n$\r$\nNh��n �Hoa�n tha�nh� ��� �o�ng Tri�nh tr�� li�."
  ${LangFileString} MUI_UNTEXT_FINISH_INFO_REBOOT "Ma�y ti�nh cu�a ba�n pha�i ����c kh��i ���ng la�i ��� hoa�n t��t vi��c g�� bo� $(^NameDA). Ba�n co� mu��n kh��i ���ng la�i ngay kh�ng?"
!endif

!ifdef MUI_FINISHPAGE | MUI_UNFINISHPAGE
  ${LangFileString} MUI_TEXT_FINISH_REBOOTNOW "Kh��i ���ng la�i ngay"
  ${LangFileString} MUI_TEXT_FINISH_REBOOTLATER "T�i mu��n kh��i ���ng la�i sau"
  ${LangFileString} MUI_TEXT_FINISH_RUN "&Cha�y $(^NameDA)"
  ${LangFileString} MUI_TEXT_FINISH_SHOWREADME "Hi��n &Readme"
  ${LangFileString} MUI_BUTTONTEXT_FINISH "&Hoa�n tha�nh"  
!endif

!ifdef MUI_STARTMENUPAGE
  ${LangFileString} MUI_TEXT_STARTMENU_TITLE "Cho�n th� mu�c Tri�nh ��n Start"
  ${LangFileString} MUI_TEXT_STARTMENU_SUBTITLE "Cho�n m��t th� mu�c tr�n Tri�nh ��n Start ��� ta�o l��i t��t cho $(^NameDA)."
  ${LangFileString} MUI_INNERTEXT_STARTMENU_TOP "Cho�n th� mu�c tr�n Tri�nh ��n Start ma� ba�n mu��n ta�o l��i t��t cho ch��ng tri�nh. Ba�n cu�ng co� th�� nh��p t�n ��� ta�o th� mu�c m��i."
  ${LangFileString} MUI_INNERTEXT_STARTMENU_CHECKBOX "Kh�ng ta�o l��i t��t"
!endif

!ifdef MUI_UNCONFIRMPAGE
  ${LangFileString} MUI_UNTEXT_CONFIRM_TITLE "G�� bo� $(^NameDA)"
  ${LangFileString} MUI_UNTEXT_CONFIRM_SUBTITLE "G�� bo� $(^NameDA) kho�i ma�y ti�nh cu�a ba�n."
!endif

!ifdef MUI_ABORTWARNING
  ${LangFileString} MUI_TEXT_ABORTWARNING "Ba�n co� th��t s�� mu��n thoa�t tri�nh Ca�i ���t $(^Name) kh�ng?"
!endif

!ifdef MUI_UNABORTWARNING
  ${LangFileString} MUI_UNTEXT_ABORTWARNING "Ba�n co� th��t s�� mu��n thoa�t tri�nh G�� bo� $(^Name) kh�ng?"
!endif

!ifdef MULTIUSER_INSTALLMODEPAGE
  ${LangFileString} MULTIUSER_TEXT_INSTALLMODE_TITLE "Cho�n ng���i du�ng"
  ${LangFileString} MULTIUSER_TEXT_INSTALLMODE_SUBTITLE "Cho�n ng���i du�ng ma� ba�n mu��n ca�i ���t $(^NameDA)."
  ${LangFileString} MULTIUSER_INNERTEXT_INSTALLMODE_TOP "Cho�n gi��a vi��c ca�i ���t $(^NameDA) cho ri�ng ba�n ho��c cho t��t ca� ng���i du�ng cu�a ma�y ti�nh na�y. $(^ClickNext)"
  ${LangFileString} MULTIUSER_INNERTEXT_INSTALLMODE_ALLUSERS "Ca�i ���t cho b��t ki� ng���i na�o s�� du�ng ma�y ti�nh na�y"
  ${LangFileString} MULTIUSER_INNERTEXT_INSTALLMODE_CURRENTUSER "Chi� ca�i ���t cho ri�ng t�i"
!endif
