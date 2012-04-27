#!/usr/bin/lua

require 'luaprocessing'

function setup()
    size(800, 500)
end

function mouseDragged()
    print('dragged', mouseX, mouseY)
end

function mouseClicked()
    print('clicked', mouseX, mouseY)
end

function mouseOver()
    print('over')
end

function mouseOut()
    print('out')
end

run()
