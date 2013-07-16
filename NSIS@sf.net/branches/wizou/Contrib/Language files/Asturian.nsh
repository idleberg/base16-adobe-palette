;Language: Asturian (1150)
;By Marcos (marcoscostales@gmail.com).

!insertmacro LANGFILE "Asturian" "Asturies"

!ifdef MUI_WELCOMEPAGE
  ${LangFileString} MUI_TEXT_WELCOME_INFO_TITLE "Bienven�u al Asistente d'Instalaci�n de $(^NameDA)"
  ${LangFileString} MUI_TEXT_WELCOME_INFO_TEXT "Esti programa instalar� $(^NameDA) nel to ordenador.$\r$\n$\r$\nEncami�ntase que zarres toles dem�s aplicaciones enantes d'aniciar la instalaci�n. Esto faer� posible anovar ficheros rellacionaos col sistema ensin tener que reaniciar el to ordenador.$\r$\n$\r$\n$_CLICK"
!endif

!ifdef MUI_UNWELCOMEPAGE
  ${LangFileString} MUI_UNTEXT_WELCOME_INFO_TITLE "Bienven�u al Asistente de Desinstalaci�n de $(^NameDA)"
  ${LangFileString} MUI_UNTEXT_WELCOME_INFO_TEXT "Esti asistente guiar�te durante la desinstalaci�n de $(^NameDA).$\r$\n$\r$\nEnantes d'aniciar la desinstalaci�n, aseg�rate de que $(^NameDA) nun se ta executando.$\r$\n$\r$\n$_CLICK"
!endif

!ifdef MUI_LICENSEPAGE
  ${LangFileString} MUI_TEXT_LICENSE_TITLE "Alcuerdu de llicencia"
  ${LangFileString} MUI_TEXT_LICENSE_SUBTITLE "Por favor revisa los t�rminos de la llicencia enantes d'instalar $(^NameDA)."
  ${LangFileString} MUI_INNERTEXT_LICENSE_BOTTOM "Si aceutes tolos t�rminos del alcuerdu, seleiciona Aceuto pa siguir. Tienes d'aceutar l'alcuerdu pa instalar $(^NameDA)."
  ${LangFileString} MUI_INNERTEXT_LICENSE_BOTTOM_CHECKBOX "Si aceutes los t�rminos del alcuerdu, conse�a embaxo la caxella. Tienes d'aceutar los t�rminos pa instalar $(^NameDA). $_CLICK"
  ${LangFileString} MUI_INNERTEXT_LICENSE_BOTTOM_RADIOBUTTONS "Si aceutes los t�rminos del alcuerdu, seleiciona embaxo la primer opci�n. Tienes d'aceutar los t�rminos pa instalar $(^NameDA). $_CLICK"
!endif

!ifdef MUI_UNLICENSEPAGE
  ${LangFileString} MUI_UNTEXT_LICENSE_TITLE "Alcuerdu de llicencia"
  ${LangFileString} MUI_UNTEXT_LICENSE_SUBTITLE "Por favor revisa los t�rminos de la llicencia enantes de desinstalar $(^NameDA)."
  ${LangFileString} MUI_UNINNERTEXT_LICENSE_BOTTOM "Si aceutes tolos t�rminos del alcuerdu, seleiciona Aceuto pa siguir. Tienes d'aceutar l'alcuerdu pa desinstalar $(^NameDA)."
  ${LangFileString} MUI_UNINNERTEXT_LICENSE_BOTTOM_CHECKBOX "Si aceutes los t�rminos del alcuerdu, conse�a embaxu la caxella. Tienes d'aceutar los t�rminos pa desinstalar $(^NameDA). $_CLICK"
  ${LangFileString} MUI_UNINNERTEXT_LICENSE_BOTTOM_RADIOBUTTONS "Si aceutes los t�rminos del alcuerdu, seleccione embaxu la primer opci�n. Tienes d'aceutar los t�rminos pa desinstalar $(^NameDA). $_CLICK"
!endif

!ifdef MUI_LICENSEPAGE | MUI_UNLICENSEPAGE
  ${LangFileString} MUI_INNERTEXT_LICENSE_TOP "Calca Avanzar P�xina pa ver el restu del alcuerdu."
!endif

!ifdef MUI_COMPONENTSPAGE
  ${LangFileString} MUI_TEXT_COMPONENTS_TITLE "Seleici�n de componentes"
  ${LangFileString} MUI_TEXT_COMPONENTS_SUBTITLE "Seleicione qu� carauter�stiques de $(^NameDA) deseyes instalar."
  ${LangFileString} MUI_INNERTEXT_COMPONENTS_DESCRIPTION_TITLE "Descripci�n"
!endif

!ifdef MUI_UNCOMPONENTSPAGE
  ${LangFileString} MUI_UNTEXT_COMPONENTS_TITLE "Seleici�n de componentes"
  ${LangFileString} MUI_UNTEXT_COMPONENTS_SUBTITLE "Seleicione qu� carauter�stiques de $(^NameDA) quies desinstalar."
!endif

!ifdef MUI_COMPONENTSPAGE | MUI_UNCOMPONENTSPAGE
  !ifndef NSIS_CONFIG_COMPONENTPAGE_ALTERNATIVE
    ${LangFileString} MUI_INNERTEXT_COMPONENTS_DESCRIPTION_INFO "Allugue'l mur enriba d'un componente pa ver la so descripci�n."
  !else
    ${LangFileString} MUI_INNERTEXT_COMPONENTS_DESCRIPTION_INFO "Allugue'l mur enriba d'un componente pa ver la so descripci�n."
  !endif
!endif

!ifdef MUI_DIRECTORYPAGE
  ${LangFileString} MUI_TEXT_DIRECTORY_TITLE "Escoyer llugar d'instalaci�n"
  ${LangFileString} MUI_TEXT_DIRECTORY_SUBTITLE "Escueye'l direutoriu pa instalar $(^NameDA)."
!endif

!ifdef MUI_UNDIRECTORYPAGE
  ${LangFileString} MUI_UNTEXT_DIRECTORY_TITLE "Escoyer llugar de desinstalaci�n"
  ${LangFileString} MUI_UNTEXT_DIRECTORY_SUBTITLE "Escueye'l direutoriu dende'l cual se desinstalar� $(^NameDA)."
!endif

!ifdef MUI_INSTFILESPAGE
  ${LangFileString} MUI_TEXT_INSTALLING_TITLE "Instalando"
  ${LangFileString} MUI_TEXT_INSTALLING_SUBTITLE "Por favor espera mientres $(^NameDA) s'instala."
  ${LangFileString} MUI_TEXT_FINISH_TITLE "Instalaci�n Completada"
  ${LangFileString} MUI_TEXT_FINISH_SUBTITLE "La instalaci�n complet�se correutamente."
  ${LangFileString} MUI_TEXT_ABORT_TITLE "Instalaci�n Desaniciada"
  ${LangFileString} MUI_TEXT_ABORT_SUBTITLE "La instalaci�n nun se complet� correutamente."
!endif

!ifdef MUI_UNINSTFILESPAGE
  ${LangFileString} MUI_UNTEXT_UNINSTALLING_TITLE "Desinstalando"
  ${LangFileString} MUI_UNTEXT_UNINSTALLING_SUBTITLE "Por favor espera mientres $(^NameDA) se desinstala."
  ${LangFileString} MUI_UNTEXT_FINISH_TITLE "Desinstalaci�n Completada"
  ${LangFileString} MUI_UNTEXT_FINISH_SUBTITLE "La desinstalaci�n complet�se correutamente."
  ${LangFileString} MUI_UNTEXT_ABORT_TITLE "Desinstalaci�n Desaniciada"
  ${LangFileString} MUI_UNTEXT_ABORT_SUBTITLE "La desinstalaci�n nun se complet� correutamente."
!endif

!ifdef MUI_FINISHPAGE
  ${LangFileString} MUI_TEXT_FINISH_INFO_TITLE "Completando l'Asistente d'Instalaci�n de $(^NameDA)"
  ${LangFileString} MUI_TEXT_FINISH_INFO_TEXT "$(^NameDA) foi instal�u nel to sistema.$\r$\n$\r$\nCalca Finar pa zarrar esti asistente."
  ${LangFileString} MUI_TEXT_FINISH_INFO_REBOOT "El to sistema tien de reaniciase pa que pueda completase la instalaci�n de $(^NameDA). �Quies reaniciar agora?"
!endif

!ifdef MUI_UNFINISHPAGE
  ${LangFileString} MUI_UNTEXT_FINISH_INFO_TITLE "Completando l'Asistente de Desinstalaci�n de $(^NameDA)"
  ${LangFileString} MUI_UNTEXT_FINISH_INFO_TEXT "$(^NameDA) foi desinstal�u del to sistema.$\r$\n$\r$\nCalca Finar pa zarrar esti asistente."
  ${LangFileString} MUI_UNTEXT_FINISH_INFO_REBOOT "El to ordenador tien que reaniciar pa finar la desinstalaci�n de $(^NameDA). �Quies reiniciar agora?"
!endif

!ifdef MUI_FINISHPAGE | MUI_UNFINISHPAGE
  ${LangFileString} MUI_TEXT_FINISH_REBOOTNOW "Reaniciar agora"
  ${LangFileString} MUI_TEXT_FINISH_REBOOTLATER "Deseo reaniciar manualmente m�s sero"
  ${LangFileString} MUI_TEXT_FINISH_RUN "&Executar $(^NameDA)"
  ${LangFileString} MUI_TEXT_FINISH_SHOWREADME "&Ver Ll�ame"
  ${LangFileString} MUI_BUTTONTEXT_FINISH "&Finar"  
!endif

!ifdef MUI_STARTMENUPAGE
  ${LangFileString} MUI_TEXT_STARTMENU_TITLE "Escoyer Carpeta del Men� Aniciu"
  ${LangFileString} MUI_TEXT_STARTMENU_SUBTITLE "Escueye una Carpeta del Men� Aniciu pa los accesos direutos de $(^NameDA)."
  ${LangFileString} MUI_INNERTEXT_STARTMENU_TOP "Seleiciona una carpeta del Men� Aniciu na que quieras criar los accesos direutos del programa. Tami�n puedes introducir un nome pa criar una nueva carpeta."
  ${LangFileString} MUI_INNERTEXT_STARTMENU_CHECKBOX "Non criar accesos direutos"
!endif

!ifdef MUI_UNCONFIRMPAGE
  ${LangFileString} MUI_UNTEXT_CONFIRM_TITLE "Desinstalar $(^NameDA)"
  ${LangFileString} MUI_UNTEXT_CONFIRM_SUBTITLE "Desaniciar $(^NameDA) del to sistema."
!endif

!ifdef MUI_ABORTWARNING
  ${LangFileString} MUI_TEXT_ABORTWARNING "�De xuru que quies colar de la instalaci�n de $(^Name)?"
!endif

!ifdef MUI_UNABORTWARNING
  ${LangFileString} MUI_UNTEXT_ABORTWARNING "�De xuru que quies colar de la desinstalaci�n de $(^Name)?"
!endif

!ifdef MULTIUSER_INSTALLMODEPAGE
  ${LangFileString} MULTIUSER_TEXT_INSTALLMODE_TITLE "Escoyer Usuarios"
  ${LangFileString} MULTIUSER_TEXT_INSTALLMODE_SUBTITLE "Escueye los usuarios pa los cuales quies instalar $(^NameDA)."
  ${LangFileString} MULTIUSER_INNERTEXT_INSTALLMODE_TOP "Seleiciona si quies instalar $(^NameDA) nam�i pa t� o pa tolos usuarios d'esti Ordenador.$(^ClickNext)"
  ${LangFileString} MULTIUSER_INNERTEXT_INSTALLMODE_ALLUSERS "Instalaci�n pa cualisquier usuariu d'esti ordenador"
  ${LangFileString} MULTIUSER_INNERTEXT_INSTALLMODE_CURRENTUSER "Instalaci�n nam�i pa m�"
!endif
