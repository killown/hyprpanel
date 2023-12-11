if get_option('vapi')
    add_languages('vala')
    executable(
        'hyprpanel',
        files('hyprpanel.vala'),
        dependencies: [gtk, gtk_layer_shell, vapi, meson.get_compiler('vala').find_library('posix')],
        install: true)
endif
