using Gtk;
using GtkLayerShell;

bool menu_running = false;

int RunAPP(string app) {
    MainLoop loop = new MainLoop();
    try {
        string[] spawn_args = { app };
        string[] spawn_env = Environ.get();
        Pid child_pid;

        Process.spawn_async("/",
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

Gtk.Widget CreateNewLauncher(string app, string icon) {
    var btn = new Button.from_icon_name(icon);
    btn.get_style_context().add_class("launcher");
    btn.halign = Gtk.Align.FILL;
    btn.valign = Gtk.Align.FILL;
    btn.clicked.connect(() => {
        RunAPP(app);
    });
    return btn;
}

int main(string[] argv) {
    var app = new Gtk.Application(
                                  "com.github.wmww.gtk4-layer-shell",
                                  GLib.ApplicationFlags.FLAGS_NONE);
    app.activate.connect(() => {

        // window setup
        var window = new Gtk.ApplicationWindow(app);
        window.set_resizable(true);
        window.get_style_context().add_class("panel");

        // config files
        string app_list = Path.build_filename(GLib.Environment.get_home_dir(), ".config/hyprpanel/app.list");
        string style_css = Path.build_filename(GLib.Environment.get_home_dir(), ".config/hyprpanel/style.css");
        string menu_config = Path.build_filename(GLib.Environment.get_home_dir(), ".config/hyprpanel/menu.cfg");


        // setup panel position
        GtkLayerShell.init_for_window(window);
        //GtkLayerShell.auto_exclusive_zone_enable(window);
        GtkLayerShell.set_margin(window, GtkLayerShell.Edge.TOP, 0);
        GtkLayerShell.set_margin(window, GtkLayerShell.Edge.BOTTOM, 0);
        GtkLayerShell.set_anchor(window, GtkLayerShell.Edge.BOTTOM, true);
        GtkLayerShell.set_layer (window, GtkLayerShell.Layer.BACKGROUND);

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




        // box containing all button icons
        Gtk.Box box = new Gtk.Box(Gtk.Orientation.HORIZONTAL, 5);

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
                box.prepend(CreateNewLauncher(icon, app_name));
            }
                
            }
        } catch (Error e) {
            print("Error: %s\n", e.message);
        }

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
        // show window
        window.set_child(box);
        window.present();
    });

    return app.run(argv);
}