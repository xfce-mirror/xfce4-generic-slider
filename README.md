# xfce4-generic-slider

Xfce4-generic-slider is a tool to help Xfce users deal with a variable which they wish to both GET and SET. The getting side is similar to what [xfce4-genmon-plugin](https://gitlab.xfce.org/panel-plugins/xfce4-genmon-plugin) does except the command's numerical output is represented visually in a slider. Dragging the slider is then used to set the value through calls to a second command.

## Homepage

Development of this plugin currently takes place at the [Xfce Gitlab](https://gitlab.xfce.org/panel-plugins/xfce4-generic-slider) site. This is where any bugs you find should be reported. Also, patches (especially translations) are most welcome!

## Installation

To clone the source tree and build the package, you need the Xfce development libraries.

    % git clone https://gitlab.xfce.org/panel-plugins/xfce4-generic-slider.git
    % cd xfce4-generic-slider
    % ./autogen.sh
    % make
    % make install

Be sure to specify an installation prefix / DESTDIR so that it goes to the right place.

## Configuration

Once xfce4-generic-slider is running in your panel, you can change the color of the slider and set whether a label appears beside it. To actually use it, you need to setup three references to the variable you're interested in. Here's an example of how to make a slider for the volume.

    Adjust this command: amixer set PCM %v
    Denominator for adjusting: 255
    Synchronize with this command: amixer get PCM | tail -1 | awk '{print $4}'
    Denominator for synchronizing: 255
    Label for slider: PCM: %v
    Denominator for label: 255

When the slider is dragged to a certain fraction, this fraction times 255 will be passed to `amixer set`. When another application changes the volume, the slider will call `amixer get` to determine what the returned number is as a fraction of 255. Note that the right denominator can differ depending on the sound card.
