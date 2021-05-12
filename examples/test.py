import pystk
from time import time
import numpy as np
import gui


def action_dict(action):
    return {k: getattr(action, k) for k in ['acceleration', 'brake', 'steer', 'fire', 'drift']}


def main():
    config = pystk.GraphicsConfig.hd()
    config.screen_width = 800
    config.screen_height = 600
    pystk.init(config)

    config = pystk.RaceConfig()
    config.num_kart = 2
    # config.track ='battleisland' 
    config.track ='stadium' 

    config.players[0].controller = pystk.PlayerConfig.Controller.PLAYER_CONTROL
    config.players[0].team = 0
    # NOTE: Add 4 players
    for _ in range(3):
        config.players.append(
                # pystk.PlayerConfig(args.kart, pystk.PlayerConfig.Controller.AI_CONTROL, (args.team + 1) % 2))
                pystk.PlayerConfig('', pystk.PlayerConfig.Controller.AI_CONTROL, 1))

    config.mode = config.RaceMode.THREE_STRIKES
    # TODO: Look at step size?
    # config.step_size = args.step_size

    race = pystk.Race(config)
    race.start()
    race.step()

    uis = [gui.UI(gui.VT['IMAGE']) for i in range(4)]

    state = pystk.WorldState()
    state.update()
    t0 = time()
    if not all(ui.pause for ui in uis):
        race.step(uis[0].current_action)
        state.update()
    for ui, d in zip(uis, race.render_data):
        ui.show(d)
    input('press enter to continue')

    race.stop()
    del race
    pystk.clean()



if __name__ == '__main__':
    main()
