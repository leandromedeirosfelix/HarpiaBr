using System.Globalization;
using System.Windows;
using System.Windows.Markup;

namespace HarpiaBuilder;

/// One of the game's languages: its code in language.txt, its own name, and the app's text in it.
sealed record Language(string Code, string Name, string[] Text)
{
    public override string ToString() => Name;
}

enum S
{
    Subtitle, GameFiles, Nup, Folders, DropHint, SaveTo, Build, Browse, Needs, Free, Checking, VersionOk, VersionBad, ToolsMissing, PythonMissing,
    Building, Patience, Elapsed, Read, Lift, Compile, Finish, ShowDetails, HideDetails, Cancel, Back, OpenFolder, Play, Ready, BuiltIn,
    Stopped, Cancelled, CancelledNote, CloseQuestion, LanguageLabel,
    // (the update, English only for now: Strings.Text falls back to it)
    Download, Updating, UpdateFailed,
}

static class Strings
{
    static readonly string[] En =
    {
        "Turns your own copy of Ocarina of Time into a game folder you can start on PC.", "Game files", ".nup file", "romfs and exefs",
        "You can also drop a file or folder onto this window.", "Save the game to", "Build", "Browse", "Takes about 45 minutes and needs 60 GB.",
        "{0} has {1} GB free.", "Checking the game version...", "This is the version Harpia supports.", "This version isn't supported. Harpia needs build e0f3ccfa.",
        "The Harpia tools were not found next to this app.", "Python was not found.",
        "Building", "This takes a while, and your PC stays usable.", "Elapsed", "Reading your game files", "Translating the game to PC code", "Compiling",
        "Putting the game folder together", "Show details", "Hide details", "Cancel", "Back", "Open folder", "Play", "Your game is ready", "built in",
        "The build stopped", "Build cancelled", "You can start again from the same folder.", "Stop the build and close?", "Language",
        "Downloading the new version", "Updating your game", "The update could not be downloaded",
    };
    static readonly string[] Fr =
    {
        "Transforme votre copie d'Ocarina of Time en un dossier de jeu à lancer sur PC.", "Fichiers du jeu", "Fichier .nup", "romfs et exefs",
        "Vous pouvez aussi déposer un fichier ou un dossier sur cette fenêtre.", "Enregistrer le jeu dans", "Créer", "Parcourir", "Prend environ 45 minutes et nécessite 60 Go.",
        "{0} : {1} Go libres.", "Vérification de la version du jeu...", "Cette version est prise en charge par Harpia.", "Cette version n'est pas prise en charge. Harpia a besoin de la version e0f3ccfa.",
        "Les outils Harpia sont introuvables à côté de cette application.", "Python est introuvable.",
        "Création en cours", "Cela prend du temps, et votre PC reste utilisable.", "Temps écoulé", "Lecture de vos fichiers de jeu", "Traduction du jeu en code PC", "Compilation",
        "Assemblage du dossier du jeu", "Afficher les détails", "Masquer les détails", "Annuler", "Retour", "Ouvrir le dossier", "Jouer", "Votre jeu est prêt", "créé en",
        "La création s'est arrêtée", "Création annulée", "Vous pouvez recommencer depuis le même dossier.", "Arrêter la création et fermer ?", "Langue",
    };
    static readonly string[] De =
    {
        "Macht aus deiner eigenen Kopie von Ocarina of Time einen Spielordner für den PC.", "Spieldateien", ".nup-Datei", "romfs und exefs",
        "Du kannst auch eine Datei oder einen Ordner auf dieses Fenster ziehen.", "Spiel speichern unter", "Erstellen", "Durchsuchen", "Dauert etwa 45 Minuten und benötigt 60 GB.",
        "{0} hat {1} GB frei.", "Spielversion wird geprüft...", "Diese Version wird von Harpia unterstützt.", "Diese Version wird nicht unterstützt. Harpia benötigt Build e0f3ccfa.",
        "Die Harpia-Werkzeuge wurden neben dieser App nicht gefunden.", "Python wurde nicht gefunden.",
        "Wird erstellt", "Das dauert eine Weile, dein PC bleibt nutzbar.", "Vergangen", "Spieldateien werden gelesen", "Spiel wird in PC-Code übersetzt", "Kompilieren",
        "Spielordner wird zusammengestellt", "Details anzeigen", "Details ausblenden", "Abbrechen", "Zurück", "Ordner öffnen", "Spielen", "Dein Spiel ist fertig", "erstellt in",
        "Die Erstellung wurde angehalten", "Erstellung abgebrochen", "Du kannst im selben Ordner neu beginnen.", "Erstellung beenden und schließen?", "Sprache",
    };
    static readonly string[] It =
    {
        "Trasforma la tua copia di Ocarina of Time in una cartella di gioco da avviare su PC.", "File di gioco", "File .nup", "romfs ed exefs",
        "Puoi anche trascinare un file o una cartella su questa finestra.", "Salva il gioco in", "Crea", "Sfoglia", "Richiede circa 45 minuti e 60 GB.",
        "{0} ha {1} GB liberi.", "Controllo della versione del gioco...", "Questa versione è supportata da Harpia.", "Questa versione non è supportata. Harpia richiede la build e0f3ccfa.",
        "Gli strumenti di Harpia non sono stati trovati accanto a questa app.", "Python non è stato trovato.",
        "Creazione in corso", "Ci vuole un po', e il PC resta utilizzabile.", "Trascorso", "Lettura dei file di gioco", "Traduzione del gioco in codice PC", "Compilazione",
        "Assemblaggio della cartella di gioco", "Mostra dettagli", "Nascondi dettagli", "Annulla", "Indietro", "Apri cartella", "Gioca", "Il tuo gioco è pronto", "creato in",
        "La creazione si è interrotta", "Creazione annullata", "Puoi ricominciare dalla stessa cartella.", "Interrompere la creazione e chiudere?", "Lingua",
    };
    static readonly string[] Es =
    {
        "Convierte tu copia de Ocarina of Time en una carpeta de juego para iniciar en PC.", "Archivos del juego", "Archivo .nup", "romfs y exefs",
        "También puedes soltar un archivo o una carpeta en esta ventana.", "Guardar el juego en", "Crear", "Examinar", "Tarda unos 45 minutos y necesita 60 GB.",
        "{0} tiene {1} GB libres.", "Comprobando la versión del juego...", "Harpia admite esta versión.", "Esta versión no es compatible. Harpia necesita la build e0f3ccfa.",
        "No se encontraron las herramientas de Harpia junto a esta aplicación.", "No se encontró Python.",
        "Creando", "Tarda un rato, y tu PC sigue siendo utilizable.", "Transcurrido", "Leyendo tus archivos del juego", "Traduciendo el juego a código de PC", "Compilando",
        "Preparando la carpeta del juego", "Mostrar detalles", "Ocultar detalles", "Cancelar", "Atrás", "Abrir carpeta", "Jugar", "Tu juego está listo", "creado en",
        "La creación se detuvo", "Creación cancelada", "Puedes empezar de nuevo desde la misma carpeta.", "¿Detener la creación y cerrar?", "Idioma",
    };
    static readonly string[] Pt =
    {
        "Transforma sua cópia de Ocarina of Time em uma pasta de jogo para iniciar no PC.", "Arquivos do jogo", "Arquivo .nup", "romfs e exefs",
        "Você também pode soltar um arquivo ou uma pasta nesta janela.", "Salvar o jogo em", "Criar", "Procurar", "Leva cerca de 45 minutos e precisa de 60 GB.",
        "{0} tem {1} GB livres.", "Verificando a versão do jogo...", "Esta versão é compatível com o Harpia.", "Esta versão não é compatível. O Harpia precisa da build e0f3ccfa.",
        "As ferramentas do Harpia não foram encontradas ao lado deste aplicativo.", "O Python não foi encontrado.",
        "Criando", "Isso leva um tempo, e seu PC continua utilizável.", "Decorrido", "Lendo seus arquivos do jogo", "Traduzindo o jogo para código de PC", "Compilando",
        "Montando a pasta do jogo", "Mostrar detalhes", "Ocultar detalhes", "Cancelar", "Voltar", "Abrir pasta", "Jogar", "Seu jogo está pronto", "criado em",
        "A criação parou", "Criação cancelada", "Você pode recomeçar a partir da mesma pasta.", "Parar a criação e fechar?", "Idioma",
    };
    static readonly string[] Nl =
    {
        "Maakt van je eigen exemplaar van Ocarina of Time een spelmap om op pc te starten.", "Spelbestanden", ".nup-bestand", "romfs en exefs",
        "Je kunt ook een bestand of map op dit venster slepen.", "Spel opslaan in", "Maken", "Bladeren", "Duurt ongeveer 45 minuten en vereist 60 GB.",
        "{0} heeft {1} GB vrij.", "Spelversie controleren...", "Deze versie wordt door Harpia ondersteund.", "Deze versie wordt niet ondersteund. Harpia heeft build e0f3ccfa nodig.",
        "De Harpia-tools zijn niet naast deze app gevonden.", "Python is niet gevonden.",
        "Bezig met maken", "Dit duurt even, en je pc blijft bruikbaar.", "Verstreken", "Je spelbestanden lezen", "Spel vertalen naar pc-code", "Compileren",
        "Spelmap samenstellen", "Details tonen", "Details verbergen", "Annuleren", "Terug", "Map openen", "Spelen", "Je spel is klaar", "gemaakt in",
        "Het maken is gestopt", "Maken geannuleerd", "Je kunt opnieuw beginnen vanuit dezelfde map.", "Maken stoppen en sluiten?", "Taal",
    };
    static readonly string[] Pl =
    {
        "Zamienia Twoją kopię Ocarina of Time w folder gry, który uruchomisz na PC.", "Pliki gry", "Plik .nup", "romfs i exefs",
        "Możesz też upuścić plik lub folder na to okno.", "Zapisz grę w", "Utwórz", "Przeglądaj", "Trwa około 45 minut i wymaga 60 GB.",
        "{0}: wolne {1} GB.", "Sprawdzanie wersji gry...", "Ta wersja jest obsługiwana przez Harpia.", "Ta wersja nie jest obsługiwana. Harpia wymaga kompilacji e0f3ccfa.",
        "Nie znaleziono narzędzi Harpia obok tej aplikacji.", "Nie znaleziono Pythona.",
        "Tworzenie", "To chwilę potrwa, a komputer pozostanie w użyciu.", "Upłynęło", "Odczytywanie plików gry", "Tłumaczenie gry na kod PC", "Kompilowanie",
        "Składanie folderu gry", "Pokaż szczegóły", "Ukryj szczegóły", "Anuluj", "Wstecz", "Otwórz folder", "Graj", "Twoja gra jest gotowa", "utworzono w",
        "Tworzenie zostało przerwane", "Anulowano tworzenie", "Możesz zacząć ponownie z tego samego folderu.", "Przerwać tworzenie i zamknąć?", "Język",
    };
    static readonly string[] Ru =
    {
        "Превращает вашу копию Ocarina of Time в папку с игрой для запуска на ПК.", "Файлы игры", "Файл .nup", "romfs и exefs",
        "Также можно перетащить файл или папку в это окно.", "Сохранить игру в", "Собрать", "Обзор", "Занимает около 45 минут и требует 60 ГБ.",
        "{0}: свободно {1} ГБ.", "Проверка версии игры...", "Эта версия поддерживается Harpia.", "Эта версия не поддерживается. Harpia нужна сборка e0f3ccfa.",
        "Инструменты Harpia не найдены рядом с этим приложением.", "Python не найден.",
        "Сборка", "Это займёт время, а компьютером можно пользоваться.", "Прошло", "Чтение файлов игры", "Перевод игры в код для ПК", "Компиляция",
        "Подготовка папки с игрой", "Показать подробности", "Скрыть подробности", "Отмена", "Назад", "Открыть папку", "Играть", "Ваша игра готова", "собрано за",
        "Сборка остановлена", "Сборка отменена", "Можно начать заново из той же папки.", "Остановить сборку и закрыть?", "Язык",
    };
    static readonly string[] Ja =
    {
        "お持ちの Ocarina of Time を、PC で起動できるゲームフォルダーに変換します。", "ゲームファイル", ".nup ファイル", "romfs と exefs",
        "ファイルやフォルダーをこのウィンドウにドロップすることもできます。", "ゲームの保存先", "ビルド", "参照", "約 45 分かかり、60 GB 必要です。",
        "{0} の空き容量: {1} GB", "ゲームのバージョンを確認しています...", "このバージョンは Harpia に対応しています。", "このバージョンには対応していません。Harpia にはビルド e0f3ccfa が必要です。",
        "このアプリの横に Harpia ツールが見つかりません。", "Python が見つかりません。",
        "ビルド中", "時間がかかりますが、PC はそのまま使えます。", "経過時間", "ゲームファイルを読み込んでいます", "ゲームを PC 用コードに変換しています", "コンパイル中",
        "ゲームフォルダーを作成しています", "詳細を表示", "詳細を隠す", "キャンセル", "戻る", "フォルダーを開く", "プレイ", "ゲームの準備ができました", "所要時間",
        "ビルドが停止しました", "ビルドをキャンセルしました", "同じフォルダーからやり直せます。", "ビルドを中止して閉じますか?", "言語",
    };
    static readonly string[] Ko =
    {
        "가지고 있는 Ocarina of Time을 PC에서 실행할 수 있는 게임 폴더로 만듭니다.", "게임 파일", ".nup 파일", "romfs 및 exefs",
        "파일이나 폴더를 이 창에 끌어다 놓을 수도 있습니다.", "게임 저장 위치", "빌드", "찾아보기", "약 45분이 걸리며 60GB가 필요합니다.",
        "{0} 여유 공간: {1}GB", "게임 버전을 확인하는 중...", "Harpia이 지원하는 버전입니다.", "지원되지 않는 버전입니다. Harpia에는 빌드 e0f3ccfa가 필요합니다.",
        "이 앱 옆에서 Harpia 도구를 찾을 수 없습니다.", "Python을 찾을 수 없습니다.",
        "빌드 중", "시간이 걸리지만 PC는 계속 사용할 수 있습니다.", "경과 시간", "게임 파일을 읽는 중", "게임을 PC 코드로 변환하는 중", "컴파일 중",
        "게임 폴더를 만드는 중", "자세히 보기", "자세히 숨기기", "취소", "뒤로", "폴더 열기", "플레이", "게임이 준비되었습니다", "소요 시간",
        "빌드가 중단되었습니다", "빌드가 취소되었습니다", "같은 폴더에서 다시 시작할 수 있습니다.", "빌드를 중단하고 닫을까요?", "언어",
    };
    static readonly string[] ZhHans =
    {
        "将你自己的 Ocarina of Time 变成可在电脑上启动的游戏文件夹。", "游戏文件", ".nup 文件", "romfs 和 exefs",
        "也可以将文件或文件夹拖放到此窗口。", "游戏保存位置", "生成", "浏览", "大约需要 45 分钟和 60 GB 空间。",
        "{0} 可用空间 {1} GB。", "正在检查游戏版本...", "Harpia 支持此版本。", "不支持此版本。Harpia 需要构建版本 e0f3ccfa。",
        "在此应用旁边找不到 Harpia 工具。", "找不到 Python。",
        "正在生成", "这需要一些时间,期间电脑仍可正常使用。", "已用时间", "正在读取游戏文件", "正在将游戏转换为电脑代码", "正在编译",
        "正在整理游戏文件夹", "显示详细信息", "隐藏详细信息", "取消", "返回", "打开文件夹", "开始游戏", "你的游戏已准备就绪", "用时",
        "生成已停止", "生成已取消", "你可以从同一文件夹重新开始。", "要停止生成并关闭吗?", "语言",
    };
    static readonly string[] ZhHant =
    {
        "將你自己的 Ocarina of Time 變成可在電腦上啟動的遊戲資料夾。", "遊戲檔案", ".nup 檔案", "romfs 和 exefs",
        "也可以將檔案或資料夾拖放到此視窗。", "遊戲儲存位置", "建置", "瀏覽", "大約需要 45 分鐘和 60 GB 空間。",
        "{0} 可用空間 {1} GB。", "正在檢查遊戲版本...", "Harpia 支援此版本。", "不支援此版本。Harpia 需要建置版本 e0f3ccfa。",
        "在此應用程式旁找不到 Harpia 工具。", "找不到 Python。",
        "正在建置", "這需要一些時間,期間電腦仍可正常使用。", "已用時間", "正在讀取遊戲檔案", "正在將遊戲轉換為電腦程式碼", "正在編譯",
        "正在整理遊戲資料夾", "顯示詳細資訊", "隱藏詳細資訊", "取消", "返回", "開啟資料夾", "開始遊戲", "你的遊戲已準備就緒", "用時",
        "建置已停止", "建置已取消", "你可以從同一資料夾重新開始。", "要停止建置並關閉嗎?", "語言",
    };

    public static readonly Language[] All =
    {
        new("en-US", "English (US)", En), new("en-GB", "English (UK)", En), new("fr", "Français", Fr), new("fr-CA", "Français (Canada)", Fr),
        new("de", "Deutsch", De), new("it", "Italiano", It), new("es", "Español (España)", Es), new("es-419", "Español (Latinoamérica)", Es),
        new("pt-BR", "Português (Brasil)", Pt), new("nl", "Nederlands", Nl), new("pl", "Polski", Pl), new("ru", "Русский", Ru),
        new("ja", "日本語", Ja), new("ko", "한국어", Ko), new("zh-Hans", "简体中文", ZhHans), new("zh-Hant", "繁體中文", ZhHant),
    };

    /// (local: Harpia) Portuguese (Brasil), whatever Windows' language: HarpiaBR is a pt-BR project. The window's own language is
    /// the PT | EN switch (Ui, builder-settings.json); this is the game's default, which the player can change in the list.
    public static Language Default() => All.First(l => l.Code == "pt-BR");
    public static Language English => All[0];
}

/// (local: Harpia) The window's own text, beside the 40 keys above: pt-BR, with English for every other language. The key is the
/// English, so the code still reads as it says; Ui.L in code, {local:L '...'} in MainWindow.xaml. Texts from the XAML are kept
/// and set again when the language changes; texts set in code follow on their next update.
static class Ui
{
    public static bool Pt { get; private set; } = Strings.Default().Code.StartsWith("pt");
    static readonly List<(WeakReference<DependencyObject> On, DependencyProperty Property, string En)> Tracked = new();

    public static string L(string en) => Pt && PtText.TryGetValue(en, out var pt) ? pt : en;
    public static string F(string en, params object?[] values) => string.Format(CultureInfo.CurrentCulture, L(en), values);

    public static void Track(DependencyObject on, DependencyProperty property, string en) => Tracked.Add((new(on), property, en));

    public static void Use(Language language)
    {
        Pt = language.Code.StartsWith("pt");
        foreach (var (on, property, en) in Tracked)
            if (on.TryGetTarget(out var o)) o.SetValue(property, L(en));
    }

    static readonly Dictionary<string, string> PtText = new()
    {
        // the window (MainWindow.xaml)
        ["Starting…"] = "Iniciando…",
        ["Build your game"] = "Monte o seu jogo",
        ["Pick your game files and where the game goes. Harpia does the rest, once."] =
            "Escolha os arquivos do seu jogo e onde ele vai ficar. A Harpia faz o resto, uma vez só.",
        ["Uses your own copy of the game (.nup). Free software (GPL-3.0)."] =
            "Usa a sua própria cópia do jogo (.nup). Software livre (GPL-3.0).",
        ["FOUND ON THIS PC"] = "ENCONTRADOS NESTE PC",
        ["Not listed?"] = "Não está na lista?",
        ["Browse…"] = "Procurar…",
        [" · or drop the file on this window · "] = " · ou solte o arquivo nesta janela · ",
        ["romfs and exefs folders"] = "pastas romfs e exefs",
        ["INSTALL THE GAME TO"] = "INSTALAR O JOGO EM",
        ["Change"] = "Alterar",
        ["Default"] = "Padrão",
        ["GAME LANGUAGE"] = "IDIOMA DO JOGO",
        ["Language"] = "Idioma",
        ["SPEED"] = "VELOCIDADE",
        ["Speed"] = "Velocidade",
        ["Automatic: one compiler per CPU thread while free memory holds it. Maximum: every thread, using more RAM."] =
            "Automático: um compilador por thread enquanto a memória livre aguenta. Máximo: todos os threads, usando mais RAM.",
        ["About 45 min"] = "Cerca de 45 min",
        ["Build the game"] = "Montar o jogo",
        ["Building your game"] = "Montando o seu jogo",
        ["Show details"] = "Mostrar detalhes",
        ["Hide details"] = "Ocultar detalhes",
        ["Cancel"] = "Cancelar",
        ["Your game is ready"] = "O seu jogo está pronto",
        ["Play"] = "Jogar",
        ["Open folder"] = "Abrir pasta",
        ["Desktop shortcut"] = "Atalho na área de trabalho",
        ["Start menu entry"] = "Item no menu Iniciar",
        ["Free up space"] = "Liberar espaço",
        ["Show folder"] = "Mostrar pasta",
        ["Checking for updates…"] = "Procurando atualizações…",
        ["Update"] = "Atualizar",
        ["Open the game folder"] = "Abrir a pasta do jogo",
        ["The build stopped"] = "A montagem parou",
        ["Nothing was changed in your game folder."] = "Nada foi alterado na pasta do seu jogo.",
        ["Copy for a bug report"] = "Copiar para relatar um problema",
        ["Back"] = "Voltar",
        ["Repair"] = "Reparar",
        ["Minimize"] = "Minimizar",
        ["Close"] = "Fechar",
        // the window's code (MainWindow.xaml.cs)
        ["Nothing else to install: Harpia's own build tools came with its installer, each one checked."] =
            "Nada mais a instalar: as ferramentas de montagem da Harpia vieram com o instalador, cada uma conferida.",
        ["Harpia's build tools are missing from its folder. Press Build and Harpia offers to download them again."] =
            "As ferramentas de montagem da Harpia não estão na pasta dela. Clique em Montar e a Harpia oferece baixá-las de novo.",
        ["Getting the update…"] = "Buscando a atualização…",
        ["Starting Harpia…"] = "Iniciando a Harpia…",
        ["Looking for your game files…"] = "Procurando os arquivos do seu jogo…",
        ["No game files found yet"] = "Nenhum arquivo do jogo encontrado ainda",
        ["Found 1 game file"] = "1 arquivo do jogo encontrado",
        ["Found {0} game files"] = "{0} arquivos do jogo encontrados",
        ["Update available"] = "Atualização disponível",
        ["Harpia {0} is available. Your game is {1}: Update builds it again from the game files kept in its folder, compiling only what changed."] =
            "A Harpia {0} está disponível. O seu jogo é {1}: Atualizar o monta de novo a partir dos arquivos guardados na pasta dele, compilando só o que mudou.",
        ["Your game is up to date"] = "O seu jogo está atualizado",
        ["Your game is {0}, as this builder. GitHub did not answer, so newer versions could not be checked."] =
            "O seu jogo é {0}, como este builder. O GitHub não respondeu, então não deu para procurar versões novas.",
        ["{0} is the latest version. Pick game files below only to build it again from scratch."] =
            "{0} é a versão mais nova. Escolha os arquivos do jogo abaixo só para montá-lo de novo do zero.",
        ["Harpia Builder {0} is available (this one is {1}): update it first, then build."] =
            "O Harpia Builder {0} está disponível (este é o {1}): atualize-o primeiro e depois monte.",
        ["Update builder"] = "Atualizar o builder",
        ["Could not check for updates (GitHub did not answer)"] = "Não deu para procurar atualizações (o GitHub não respondeu)",
        ["Automatic updates are off"] = "Atualizações automáticas desligadas",
        ["Update available: {0}"] = "Atualização disponível: {0}",
        ["{0} is out: build this folder again with the Harpia Builder to get it"] =
            "Saiu a {0}: monte esta pasta de novo com o Harpia Builder para tê-la",
        ["Up to date with this builder (GitHub did not answer)"] = "Atualizado em relação a este builder (o GitHub não respondeu)",
        ["Up to date"] = "Atualizado",
        ["Using the folders {0} and {1} (as last time)"] = "Usando as pastas {0} e {1} (como da última vez)",
        ["Last time"] = "Da última vez",
        ["Game files"] = "Arquivos do jogo",
        ["Build"] = "Montar",
        ["No .nup files in Downloads, Desktop, Documents or the top folders of your drives. Browse to yours, or drop it on this window."] =
            "Nenhum arquivo .nup em Downloads, Área de Trabalho, Documentos ou nas pastas principais dos seus discos. Procure o seu ou solte-o nesta janela.",
        ["Checking…"] = "Conferindo…",
        ["✓ Supported version"] = "✓ Versão compatível",
        ["Not the supported version"] = "Não é a versão compatível",
        ["Can't be read"] = "Não pode ser lido",
        ["Needs 60 GB · {0} has {1} GB free"] = "Precisa de 60 GB · {0} tem {1} GB livres",
        ["Needs 60 GB"] = "Precisa de 60 GB",
        ["Game file (*.nup)|*.nup"] = "Arquivo do jogo (*.nup)|*.nup",
        ["Choose your game file"] = "Escolha o arquivo do seu jogo",
        ["Choose the folder with romfs and exefs (or the romfs folder)"] = "Escolha a pasta com romfs e exefs (ou a pasta romfs)",
        ["Now choose the exefs folder"] = "Agora escolha a pasta exefs",
        ["That exefs folder has no file named main."] = "Essa pasta exefs não tem um arquivo chamado main.",
        ["Using the folders {0} and {1}"] = "Usando as pastas {0} e {1}",
        ["Choose where the game goes"] = "Escolha onde o jogo vai ficar",
        ["Choose where Harpia keeps its build files"] = "Escolha onde a Harpia guarda os arquivos de montagem",
        ["Harpia cannot keep its build files there: {0}"] = "A Harpia não pode guardar os arquivos de montagem aí: {0}",
        ["Build files: about {0}, kept in {1} so that updates are quick. They can be freed after the build."] =
            "Arquivos de montagem: cerca de {0}, guardados em {1} para as atualizações serem rápidas. Podem ser liberados depois da montagem.",
        [" {0} has only {1} free."] = " {0} tem só {1} livres.",
        ["Getting the new builder…"] = "Buscando o builder novo…",
        [" You can also download HarpiaBuilder.exe from the releases page."] = " Você também pode baixar o HarpiaBuilder.exe na página de releases.",
        ["The build tools are not in Harpia's folder."] = "As ferramentas de montagem não estão na pasta da Harpia.",
        ["Download the update"] = "Baixar a atualização",
        ["Read your game files"] = "Ler os arquivos do seu jogo",
        ["Read the game files kept in the folder"] = "Ler os arquivos do jogo guardados na pasta",
        ["Translate the game to PC code"] = "Traduzir o jogo para código de PC",
        ["Compile"] = "Compilar",
        ["Compile changed files"] = "Compilar os arquivos que mudaram",
        ["Put the game together"] = "Montar a pasta do jogo",
        ["Swap in the new game"] = "Trocar pelo jogo novo",
        ["Your game files are kept: the .nup isn't needed"] = "Os arquivos do seu jogo estão guardados: o .nup não é necessário",
        ["{0} of {1} files"] = "{0} de {1} arquivos",
        ["{0} of {1} changed files"] = "{0} de {1} arquivos que mudaram",
        ["Everything is already built"] = "Tudo já está montado",
        ["{0} compilers"] = "{0} compiladores",
        [". Your current game stays as it is until the new one is complete."] = ". O seu jogo atual fica como está até o novo ficar pronto.",
        [" · your PC stays usable"] = " · o seu PC continua utilizável",
        ["Estimating…"] = "Calculando…",
        ["Compiling {0} · {1}{2}"] = "Compilando {0} · {1}{2}",
        ["Working…"] = "Trabalhando…",
        ["Usually about 45 minutes"] = "Normalmente cerca de 45 minutos",
        ["Less than a minute left"] = "Falta menos de um minuto",
        ["About a minute left"] = "Falta cerca de um minuto",
        ["About {0} h {1} min left"] = "Faltam cerca de {0} h {1} min",
        ["About {0} minutes left"] = "Faltam cerca de {0} minutos",
        ["The build tools stopped without saying why."] = "As ferramentas de montagem pararam sem dizer por quê.",
        ["Your game is updated"] = "O seu jogo foi atualizado",
        [" (compiled in {0})"] = " (compilado em {0})",
        ["Updated in {0}{1}{2} · {3}"] = "Atualizado em {0}{1}{2} · {3}",
        ["Built in {0}{1}{2} · {3}"] = "Montado em {0}{1}{2} · {3}",
        ["Harpia Builder keeps {0} on this PC ({1}). {2} of it only makes the next update quicker and can be freed: your game keeps playing, and the next update then takes about as long as this build."] =
            "O Harpia Builder ocupa {0} neste PC ({1}). {2} disso só deixa a próxima atualização mais rápida e pode ser liberado: o seu jogo continua funcionando, e a próxima atualização demora mais ou menos o mesmo que esta montagem.",
        ["Harpia Builder keeps {0} on this PC ({1}): the builder itself."] = "O Harpia Builder ocupa {0} neste PC ({1}): é o próprio builder.",
        ["Freeing…"] = "Liberando…",
        [" ({0} could not be deleted: a file is in use. Close the game and the builder, and delete the folder's contents by hand.)"] =
            " ({0} não pôde ser apagado: um arquivo está em uso. Feche o jogo e o builder e apague o conteúdo da pasta à mão.)",
        ["{0} h {1} min"] = "{0} h {1} min",
        ["{0} minutes"] = "{0} minutos",
        ["{0} seconds"] = "{0} segundos",
        ["about the same"] = "mais ou menos o mesmo",
        ["{0} faster"] = "{0} mais rápido",
        ["{0} slower"] = "{0} mais devagar",
        [" · last time {0} ({1})"] = " · da última vez {0} ({1})",
        ["Your current game is unchanged and still plays."] = "O seu jogo atual não mudou e continua funcionando.",
        ["The build tools need repairing"] = "As ferramentas de montagem precisam de reparo",
        ["Your antivirus removed part of the build tools"] = "O seu antivírus removeu parte das ferramentas de montagem",
        ["Antivirus programs sometimes flag unsigned developer tools by mistake."] =
            "Às vezes os antivírus marcam por engano ferramentas de desenvolvimento sem assinatura.",
        ["Open Windows Security → Protection history, and restore the removed files."] =
            "Abra Segurança do Windows → Histórico de proteção e restaure os arquivos removidos.",
        ["Or add {0} to its exclusions."] = "Ou adicione {0} às exclusões dele.",
        ["Then press Repair: Harpia downloads the tools again from their official releases and checks each one."] =
            "Depois clique em Reparar: a Harpia baixa as ferramentas de novo das versões oficiais e confere cada uma.",
        ["The update stopped"] = "A atualização parou",
        ["What went wrong"] = "O que deu errado",
        ["Visual Studio Build Tools or the Windows SDK is missing"] = "Faltam o Visual Studio Build Tools ou o Windows SDK",
        ["Harpia compiles the game with Microsoft's C++ compiler, which Microsoft does not allow to be bundled."] =
            "A Harpia compila o jogo com o compilador C++ da Microsoft, que a Microsoft não permite distribuir junto.",
        ["Install Visual Studio Build Tools: visualstudio.microsoft.com/downloads (Tools for Visual Studio)."] =
            "Instale o Visual Studio Build Tools: visualstudio.microsoft.com/downloads (Ferramentas para o Visual Studio).",
        ["In its installer, choose \"Desktop development with C++\" and keep the Windows SDK it selects."] =
            "No instalador, escolha \"Desenvolvimento para desktop com C++\" e mantenha o Windows SDK que ele marca.",
        ["Then press Build again: nothing needs to be downloaded twice."] = "Depois clique em Montar de novo: nada precisa ser baixado duas vezes.",
        ["Copied"] = "Copiado",
        ["the latest version"] = "a versão mais nova",
        ["Updating your game"] = "Atualizando o seu jogo",
        ["The update could not be downloaded"] = "A atualização não pôde ser baixada",
        ["Build tools checked"] = "Ferramentas de montagem conferidas",
        ["Stop the build and close?"] = "Parar a montagem e fechar?",
        // the PC check (PcCheck.cs)
        ["PC CHECK"] = "VERIFICAÇÃO DO PC",
        ["PC check:"] = "Verificação do PC:",
        ["modified today"] = "modificado hoje",
        ["modified yesterday"] = "modificado ontem",
        ["{0} days ago"] = "há {0} dias",
        ["{0} weeks ago"] = "há {0} semanas",
        ["{0} months ago"] = "há {0} meses",
        ["Desktop"] = "Área de trabalho",
        ["Documents"] = "Documentos",
        ["Home"] = "Pasta pessoal",
        ["Recommended speed: 1 compiler"] = "Velocidade recomendada: 1 compilador",
        // NVIDIA DLSS, optional (MainWindow.Dlss.cs, NvidiaDlss.cs)
        ["Enable NVIDIA DLSS (optional)"] = "Ativar NVIDIA DLSS (opcional)",
        ["Remove DLSS"] = "Remover DLSS",
        ["NVIDIA DLSS needs an NVIDIA RTX card (this PC: {0})."] = "O NVIDIA DLSS precisa de uma placa NVIDIA RTX (este PC: {0}).",
        ["NVIDIA DLSS (optional)"] = "NVIDIA DLSS (opcional)",
        ["NVIDIA terms"] = "Termos da NVIDIA",
        ["I have read and accept the NVIDIA terms"] = "Li e aceito os termos da NVIDIA",
        ["NVIDIA DLSS is on in {0}."] = "O NVIDIA DLSS está ativo em {0}.",
        ["Remove DLSS deletes only NVIDIA's files the Builder copied (9 DLLs and their 2 licence files). The game keeps working, without DLSS."] =
            "Remover DLSS apaga só os arquivos da NVIDIA que o Builder copiou (9 DLLs e os 2 arquivos de licença). O jogo continua funcionando, sem DLSS.",
        ["NVIDIA's DLSS sharpens the image and raises the frame rate on RTX cards; Frame Generation (RTX 40 and newer) shows extra frames."] =
            "O DLSS da NVIDIA deixa a imagem mais nítida e aumenta o fps em placas RTX; a Geração de Quadros (RTX 40 em diante) mostra quadros extras.",
        ["It comes from NVIDIA's official package (Streamline {0}, {1} MB, github.com/NVIDIA-RTX/Streamline), checked by its SHA-256, and goes only into your game folder. NVIDIA's terms are shown before any file is copied: you read and accept them yourself."] =
            "Ele vem do pacote oficial da NVIDIA (Streamline {0}, {1} MB, github.com/NVIDIA-RTX/Streamline), conferido pelo SHA-256, e vai só para a pasta do seu jogo. Os termos da NVIDIA aparecem antes de qualquer arquivo ser copiado: você mesmo lê e aceita.",
        ["Continue"] = "Continuar",
        ["Download from NVIDIA ({0} MB)"] = "Baixar da NVIDIA ({0} MB)",
        ["Copying NVIDIA's files and checking their signatures…"] = "Copiando os arquivos da NVIDIA e conferindo as assinaturas…",
        ["NVIDIA DLSS is on. In the game, open the menu (Insert) and choose DLSS in Graphics."] =
            "O NVIDIA DLSS está ativo. No jogo, abra o menu (Insert) e escolha o DLSS em Gráficos.",
        ["NVIDIA DLSS could not be installed ({0}). The game works without it."] = "Não foi possível instalar o NVIDIA DLSS ({0}). O jogo funciona sem ele.",
        ["DLSS removed. The game works as before, without DLSS."] = "DLSS removido. O jogo funciona como antes, sem DLSS.",
        ["DLSS could not be removed ({0}). Close the game and try again."] = "Não foi possível remover o DLSS ({0}). Feche o jogo e tente de novo.",
        ["Cancel download"] = "Cancelar download",
        ["Downloading NVIDIA's package…"] = "Baixando o pacote da NVIDIA…",
        ["Downloading NVIDIA's package… {0:0} of {1} MB"] = "Baixando o pacote da NVIDIA… {0:0} de {1} MB",
        ["Checking the package…"] = "Conferindo o pacote…",
        ["NVIDIA's package is here and checked. Read NVIDIA's terms below: DLSS is installed only if you accept them."] =
            "O pacote da NVIDIA chegou e foi conferido. Leia os termos da NVIDIA abaixo: o DLSS só é instalado se você aceitar.",
        ["Enable DLSS"] = "Ativar DLSS",
        ["Download cancelled. Nothing was installed."] = "Download cancelado. Nada foi instalado.",
        ["Could not reach NVIDIA's download (no internet, or GitHub did not answer). Nothing was installed: the game works without DLSS."] =
            "Não foi possível acessar o download da NVIDIA (sem internet, ou o GitHub não respondeu). Nada foi instalado: o jogo funciona sem DLSS.",
        ["The download failed ({0}). Nothing was installed: the game works without DLSS."] = "O download falhou ({0}). Nada foi instalado: o jogo funciona sem DLSS.",
        ["Try again"] = "Tentar de novo",
        ["Done"] = "Concluir",
        ["The NVIDIA package did not pass the check (its SHA-256 differs). Nothing was installed: the game works without DLSS."] =
            "O pacote da NVIDIA não passou na conferência (o SHA-256 é diferente). Nada foi instalado: o jogo funciona sem DLSS.",
        ["{0} is missing from NVIDIA's package."] = "{0} não está no pacote da NVIDIA.",
        ["{0} is not signed by NVIDIA Corporation. Nothing was installed: the game works without DLSS."] =
            "{0} não tem a assinatura da NVIDIA Corporation. Nada foi instalado: o jogo funciona sem DLSS.",
        ["Automatic"] = "Automático",
        ["Maximum"] = "Máximo",
        ["Your game is {0}, the same version as this builder."] = "O seu jogo é a versão {0}, a mesma deste builder.",
        ["Check again"] = "Verificar de novo",
        ["Checking this PC…"] = "Verificando este PC…",
        ["Processor"] = "Processador",
        ["unknown"] = "desconhecido",
        ["cores"] = "núcleos",
        ["CPU speed"] = "Velocidade da CPU",
        ["{0:0} single · {1:0} all threads (points)"] = "{0:0} em 1 thread · {1:0} em todos (pontos)",
        ["Memory"] = "Memória",
        ["{0:0.0} GB total · {1:0.0} GB free"] = "{0:0.0} GB no total · {1:0.0} GB livres",
        ["Build drive"] = "Disco da montagem",
        ["{0} · {1} · {2} GB free (needs 60)"] = "{0} · {1} · {2} GB livres (precisa de 60)",
        ["Drive speed"] = "Velocidade do disco",
        ["write {0:0} MB/s · read {1:0} MB/s · {2:0} random reads/s"] = "escrita {0:0} MB/s · leitura {1:0} MB/s · {2:0} leituras aleatórias/s",
        ["Graphics card"] = "Placa de vídeo",
        ["driver"] = "driver",
        ["Compiler and Windows SDK"] = "Compilador e Windows SDK",
        ["found"] = "encontrados",
        ["Won't work on this PC: fix the item marked ✗ first."] = "Não vai funcionar neste PC: resolva primeiro o item marcado com ✗.",
        ["Will compile, but slowly."] = "Vai compilar, mas devagar.",
        ["Compiles fine on this PC."] = "Compila bem neste PC.",
        ["About {0:0} minutes."] = "Cerca de {0:0} minutos.",
        ["Automatic: about {0:0} min · Maximum: about {1:0} min."] = "Automático: cerca de {0:0} min · Máximo: cerca de {1:0} min.",
        ["About {0:0} min"] = "Cerca de {0:0} min",
        ["About {0:0} minutes in all"] = "Cerca de {0:0} minutos no total",
        ["This folder already holds a Harpia game: only the game's program (harpia.exe and its libraries) and image.bin are replaced. Your settings, saves, shader caches and NVIDIA's DLLs stay."] =
            "Esta pasta já tem um jogo Harpia: só o programa do jogo (harpia.exe e suas bibliotecas) e o image.bin são trocados. Suas configurações, saves, caches de shaders e as DLLs da NVIDIA ficam.",
        ["Update the game in this folder"] = "Atualizar o jogo nesta pasta",
        ["Recommended speed: {0} compilers"] = "Velocidade recomendada: {0} compiladores",
    };
}

/// {local:L 'English text'} in XAML: the text in the window's language, set again when it changes (Ui.Use)
[MarkupExtensionReturnType(typeof(string))]
public sealed class L : MarkupExtension
{
    public L(string text) => Text = text;

    [ConstructorArgument("text")]
    public string Text { get; set; }

    public override object ProvideValue(IServiceProvider services)
    {
        if (services.GetService(typeof(IProvideValueTarget)) is IProvideValueTarget { TargetObject: DependencyObject on, TargetProperty: DependencyProperty property })
            Ui.Track(on, property, Text);
        return Ui.L(Text);
    }
}
