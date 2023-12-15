using Gtk;
using GtkLayerShell;


bool menu_running = false;
Button last_button;
string last_icon;

int RunAPP(string app) {
    MainLoop loop = new MainLoop();
    
    try {
        string[] spawn_args = {  };

        foreach (string a in app.split(" ")) {
           spawn_args += a ;
        }         
        string[] spawn_env = Environ.get();
        Pid child_pid;

        Process.spawn_async(GLib.Environment.get_home_dir(),
                            spawn_args,
                            spawn_env,
                            SpawnFlags.SEARCH_PATH | SpawnFlags.DO_NOT_REAP_CHILD,
                            null,
                            out child_pid);

        ChildWatch.add(child_pid, (pid, status) => {
            // Triggered when the child indicated by child_pid exits
            Process.close_pid(pid);
            loop.quit();
        });

        loop.run();
    } catch (SpawnError e) {
        print("Error: %s\n", e.message);
    }
    return 0;
}

Gtk.Widget CreateMenu(string menu) {
    var Menu = new Button.from_icon_name("whiskermenu-manjaro");
    Menu.get_style_context().add_class("launcher");
    Menu.halign = Gtk.Align.FILL;
    Menu.valign = Gtk.Align.FILL;
    Menu.clicked.connect(() => {
    GLib.Process.spawn_command_line_sync(menu);
    });
    return Menu;
}

Gtk.Widget CreateNewLauncher(string app, string icon, string css_class, bool change_icon_on_click) {
    string movewindowscript_path = Path.build_filename(GLib.Environment.get_home_dir(), ".config/hypr/scripts/mvallwinfromsameapp.py ");
    string movewindowscript =  movewindowscript_path + " " + icon;
    var btn = new Button.from_icon_name(icon);
    btn.get_style_context().add_class(css_class);
    btn.halign = Gtk.Align.FILL;
    btn.valign = Gtk.Align.FILL;

    if(change_icon_on_click == true){
        btn.clicked.connect(() => {
        if(last_icon != ""){
            last_button.set_icon_name(last_icon);
        }
        last_icon = icon;
        last_button = btn;
        btn.set_icon_name("carousel-arrow-back-symbolic");
        GLib.Process.spawn_command_line_sync(movewindowscript);
        RunAPP(app);
        GLib.Process.spawn_command_line_sync(movewindowscript);
    });
    }
    else{
        btn.clicked.connect(() => {
        GLib.Process.spawn_command_line_sync(movewindowscript);
        RunAPP(app);
        GLib.Process.spawn_command_line_sync(movewindowscript);
    });
    }

    return btn;
}

string LoadStyle(string style_css){
        // load style.css
        var provider = new Gtk.CssProvider ();
        if (FileUtils.test (style_css, FileTest.EXISTS))
        {
            try {
                provider.load_from_path(style_css);
                Gtk.StyleContext.add_provider_for_display(Gdk.Display.get_default(), provider, Gtk.STYLE_PROVIDER_PRIORITY_USER);
            } catch (Error e) {
                error ("Cannot load CSS stylesheet: %s", e.message);
            }
        }
        return style_css;
}


string LoadIcons(string app_list, string css_class, bool change_icon_on_click, Gtk.Box box){
        File file = File.new_for_path(app_list);
        try {
            FileInputStream @is = file.read();
            DataInputStream dis = new DataInputStream(@is);
            string line;

            while ((line = dis.read_line()) != null) {
            //empity lines is not allowed here
            if(line != ""){
                string icon = line.split(":")[0];
                string app_name = line.split(":")[1];
                box.prepend(CreateNewLauncher(icon, app_name, css_class, change_icon_on_click));
            }
                
            }
        } catch (Error e) {
            print("Error: %s\n", e.message);
        }
        return app_list;
}


int main(string[] argv) {
    var app = new Gtk.Application(
                                  "com.github.wmww.gtk4-layer-shell",
                                  GLib.ApplicationFlags.FLAGS_NONE);
    app.activate.connect(() => {

        // left bar setup
        var left_bar = new Gtk.ApplicationWindow(app);
        left_bar.set_resizable(true);
        left_bar.get_style_context().add_class("panel");


        // bottom bar setup
        var bottom_bar = new Gtk.ApplicationWindow(app);
        bottom_bar.set_resizable(true);
        bottom_bar.get_style_context().add_class("bottombar");
        
        
        // config files
        string app_list = Path.build_filename(GLib.Environment.get_home_dir(), ".config/hyprpanel/app.list");
        string workspace_list = Path.build_filename(GLib.Environment.get_home_dir(), ".config/hyprpanel/workspace.list");
        string style_css = Path.build_filename(GLib.Environment.get_home_dir(), ".config/hyprpanel/style.css");
        string menu_config = Path.build_filename(GLib.Environment.get_home_dir(), ".config/hyprpanel/menu.cfg");


        // setup left bar position
        GtkLayerShell.init_for_window(left_bar);
        GtkLayerShell.auto_exclusive_zone_enable(left_bar);
        GtkLayerShell.set_margin(left_bar, GtkLayerShell.Edge.TOP, 0);
        GtkLayerShell.set_margin(left_bar, GtkLayerShell.Edge.BOTTOM, 0);
        GtkLayerShell.set_anchor(left_bar, GtkLayerShell.Edge.LEFT, true);
        //GtkLayerShell.set_layer (left_bar, GtkLayerShell.Layer.BACKGROUND);


        // setup bottom bar position
        GtkLayerShell.init_for_window(bottom_bar);
        GtkLayerShell.auto_exclusive_zone_enable(bottom_bar);
        GtkLayerShell.set_margin(bottom_bar, GtkLayerShell.Edge.TOP, 0);
        GtkLayerShell.set_margin(bottom_bar, GtkLayerShell.Edge.LEFT, 0);
        GtkLayerShell.set_anchor(bottom_bar, GtkLayerShell.Edge.RIGHT, true);
        //GtkLayerShell.set_layer (bottom_bar, GtkLayerShell.Layer.BACKGROUND);




        LoadStyle(style_css);


        // box containing all button icons
        Gtk.Box box = new Gtk.Box(Gtk.Orientation.VERTICAL, 5);
        box.get_style_context().add_class("box");

        //box for bottom bar
        Gtk.Box box_bottom_bar = new Gtk.Box(Gtk.Orientation.VERTICAL, 5);
        box_bottom_bar.get_style_context().add_class("BoxBottomBar");

        LoadIcons(app_list, "LeftBar", false, box);
        LoadIcons(workspace_list, "BottomBar", true, box_bottom_bar);




       File file2 = File.new_for_path(menu_config);
       try {
        FileInputStream @is = file2.read();
        DataInputStream dis = new DataInputStream(@is);
        string menu;

        while ((menu = dis.read_line()) != null) {
            //empity lines is not allowed here
             if(menu != "" ){
                box.prepend(CreateMenu(menu));
            }
        }
    } catch (Error e) {
        print("Error: %s\n", e.message);
    }
        // show left bar
        left_bar.set_child(box);
        left_bar.present();
        // show bottom bar
        bottom_bar.set_child(box_bottom_bar);
        bottom_bar.present();
    });

    return app.run(argv);
}