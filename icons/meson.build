sizes = [16, 24, 32, 48, 64, 128]

foreach size : sizes
  install_data(
    '@0@x@0@'.format(size) / 'org.xfce.panel.genericslider.png',
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'apps',
  )
endforeach

install_data(
  'scalable' / 'org.xfce.panel.genericslider.svg',
  install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
