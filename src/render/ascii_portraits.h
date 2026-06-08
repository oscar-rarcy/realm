#pragma once

#include <array>

#include "realm.h"

namespace ascii_hud {

constexpr int PortraitRows = 5;
using Portrait = std::array<const char*, PortraitRows>;

inline Portrait entityPortrait(EntityType type) {
    switch (type) {
    case E_PEASANT:      return { "     _o_      ", "    /|\\      ", "   / | \\     ", "    / \\      ", "   hoe sack   " };
    case E_MILITIA:      return { "     o        ", "    /|]       ", "   / |        ", "    / \\      ", "   sword      " };
    case E_ARCHER:       return { "     o        ", "    /|\\)     ", "   / |  )     ", "    / \\)     ", "   arrows     " };
    case E_KNIGHT:       return { "   _o==       ", " _/|_\\       ", "/_ horse\\    ", "  /____\\     ", "  /_/ \\_\\   " };
    case E_SPEARMAN:     return { "     o   /    ", "    /|--/     ", "   / | /      ", "    / \\      ", "   spear      " };
    case E_CATAPULT:     return { "      ___/    ", "  ___/__/     ", " /_/ O  O     ", "   sling      ", "  boulder     " };
    case E_TREBUCHET:    return { "      /|      ", "     /_|__    ", "    /  |  \\  ", "   /___|___\\ ", "     O   O    " };
    case E_FISHING_BOAT: return { "      /\\      ", "  ___/  \\__  ", " /__ net __\\ ", " ~~~~~~~~~~~  ", " fishing boat " };
    case E_WARSHIP:      return { "     /\\^      ", " ___/  \\___  ", "|== o  o ==|  ", " \\________/  ", " warship      " };
    case E_TRANSPORT:    return { "     /\\       ", " ___/  \\___  ", "|_[]_[]_[]_|  ", " \\________/  ", " transport    " };
    case E_RAM:          return { "    ______    ", "  _/====/\\_  ", " <__ RAM __>  ", "   O    O     ", " battering    " };
    case E_TOWNHALL:     return { "    /\\  /\\   ", "   /__\\/__\\  ", "  | []  [] |  ", "  |  HALL  |  ", "  |__|__|__|  " };
    case E_HOUSE:        return { "      /\\      ", "     /__\\     ", "    | [] |    ", "    |____|    ", "     home     " };
    case E_BARRACKS:     return { "   /\\____/\\  ", "  /__\\__/__\\ ", "  | []  [] |  ", "  | swords |  ", "  |________|  " };
    case E_STABLE:       return { "    ______    ", "   /_hay_\\   ", "  |  __  |    ", "  |_/  \\_|   ", "   stable     " };
    case E_TOWER:        return { "     /\\       ", "    /__\\      ", "    |##|      ", "    |##|      ", "   _|__|_     " };
    case E_FARM:         return { "  ~~~~~~~~~   ", "  |%|%|%|%|   ", "  |%|%|%|%|   ", "  ~~~~~~~~~   ", "    farm      " };
    case E_BLACKSMITH:   return { "    ____      ", "   /_sm_\\    ", "  | anvil |   ", "  |  fire |   ", "  |______|    " };
    case E_CHURCH:       return { "      +       ", "     /\\      ", "    /__\\     ", "    |  |      ", "   _|__|_     " };
    case E_MARKET:       return { "   _______    ", "  /_/|_|\\_\\ ", "  | goods |   ", "  |__[]__|    ", "   market     " };
    case E_WALL:         return { "  [][][][][]  ", "  [][][][][]  ", "  [][][][][]  ", "  [][][][][]  ", "    wall      " };
    case E_GATE:         return { "  []  __  []  ", "  [] |  | []  ", "  [] |  | []  ", "  []_|__|_[]  ", "    gate      " };
    case E_CASTLE:       return { "  []  []  []  ", "  ||__||__||  ", "  |  ____  |  ", "  | |    | |  ", "  |_|____|_|  " };
    case E_LUMBER_CAMP:  return { "    _T_T_     ", "   / logs\\   ", "  |__axe__|   ", "  ||||||||    ", " lumber camp  " };
    case E_MINING_CAMP:  return { "    /\\^       ", "   /__\\ pick ", "  | ore  |    ", "  |______|    ", " mining camp  " };
    case E_MILL:         return { "     \\ | /    ", "   --- O ---  ", "     / | \\   ", "    |____|    ", "     mill     " };
    case E_DOCK:         return { "  | | | |     ", "  =========   ", "  ~ boat ~    ", "  ~~~~~~~~    ", "     dock     " };
    case E_DEER:         return { "   \\ /        ", "    Y         ", "   /|\\_      ", "  /_   \\     ", "    deer      " };
    case E_WOLF:         return { "    /\\_/\\    ", "   ( o.o )    ", "    \\_^_/    ", "    /   \\    ", "     wolf     " };
    case E_SHEEP:        return { "   .-''''-.   ", "  (  o  o )   ", "   '(____)'   ", "     ||       ", "    sheep     " };
    case E_BOAR:         return { "   __,_,__    ", "  /  o o  \\  ", " <   (..)  >  ", "   \\_/--\\_/ ", "     boar     " };
    case E_WOODEN_BRIDGE:return { "  =========   ", "  |||||||||   ", "  =========   ", "  ~~~~~~~~    ", " wood bridge  " };
    case E_STONE_BRIDGE: return { "  /=======\\   ", " [__][__][_]  ", "  \\=======/  ", "  ~~~~~~~~    ", " stone bridge " };
    default:             return { "              ", "      ?       ", "     /|\\     ", "     / \\     ", "  unknown     " };
    }
}

} // namespace ascii_hud
