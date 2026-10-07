"""Genera los esquemáticos SVG del repo con schemdraw (símbolos IEC).
Uso: pip install schemdraw && python3 gen_circuitos.py"""
import os
import schemdraw
schemdraw.use('svg')
import schemdraw.elements as elm
from schemdraw.elements import IcPin

elm.style(elm.STYLE_IEC)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'img')
os.makedirs(OUT, exist_ok=True)

CLARO = '#1f2328'
OSCURO = '#e6edf3'


def circuito_logger(fname, color):
    """DS18B20 (digital) + potenciómetro (analógico): los dos canales del logger."""
    with schemdraw.Drawing(file=fname, show=False) as d:
        d.config(color=color, lw=1.6, fontsize=11)
        # --- potenciómetro: extremos a 3V3 y GND, cursor al ADC ---
        d.add(elm.Vdd().at((0, 1.4)).label('3V3'))
        pote = d.add(elm.Potentiometer().at((0, 1.4)).down().length(2.8))
        d.add(elm.Label().at((-1.15, 0)).label('P1\n10 kΩ'))
        d.add(elm.Ground().at(pote.end))
        d.add(elm.Line().at(pote.tap).right().length(1.6))
        d.add(elm.Dot(open=True).label('GPIO 34\n(ADC1)', loc='right'))
        # --- DS18B20 con pull-up ---
        ic = d.add(elm.Ic(pins=[IcPin(name='VDD', side='top'),
                                IcPin(name='DQ', side='left'),
                                IcPin(name='GND', side='bottom')],
                          edgepadW=1.8, edgepadH=.6).label('DS18B20', 'center')
                   .at((13, 0)).right())
        d.add(elm.Line().at(ic.DQ).left().length(1.4))
        nodo = d.add(elm.Dot())
        d.add(elm.Resistor().up().at(nodo.center).length(2.2).label('R1\n4,7 kΩ'))
        d.add(elm.Vdd().label('3V3'))
        d.add(elm.Line().at(nodo.center).left().length(1.6))
        d.add(elm.Dot(open=True).label('GPIO 4', loc='left'))
        d.add(elm.Line().at(ic.VDD).up().length(.6))
        d.add(elm.Vdd().label('3V3'))
        d.add(elm.Line().at(ic.GND).down().length(.6))
        d.add(elm.Ground())


circuito_logger(f'{OUT}/circuito-logger.svg', CLARO)
circuito_logger(f'{OUT}/circuito-logger-dark.svg', OSCURO)
print('circuito-logger OK')
