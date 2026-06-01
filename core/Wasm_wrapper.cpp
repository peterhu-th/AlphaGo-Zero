#include <emscripten/bind.h>
#include "GameInterface.h"
#include "games/GoGame.h"
#include "games/GomokuGame.h"

using namespace emscripten;

class WasmGameWrapper {
    std::unique_ptr<GameInterface> game_;
    int board_size_;

public:
    WasmGameWrapper(std::string mode, int board_size) {
        board_size_ = board_size;
        if (mode == "go") {
            game_ = std::make_unique<GoGame>(board_size, 0, 0, 3.5, 9999);
        } else {
            game_ = std::make_unique<GomokuGame>(board_size);
        }
    }

    bool play_move(int x, int y, int player_val) {
        int action = -1;
        if (x == -1 && y == -1) {
            action = board_size_ * board_size_;
        } else {
            action = y * board_size_ + x;
        }

        // 检查合法性
        auto legal_moves = game_->GetLegalMoves();
        if (action >= 0 && action < legal_moves.size() && legal_moves[action] == 1) {
            game_->Step(action);
            return true;
        }
        return false;
    }

    // 从 history 数组（表示 [x,y] 的序列）同步状态
    void sync_state(const val& history) {
        game_->Reset();
        unsigned int len = history["length"].as<unsigned int>();
        for(unsigned int i = 0; i < len; ++i) {
            val move = history[i];
            int x = move[0].as<int>();
            int y = move[1].as<int>();
            int action = (x == -1 && y == -1) ? board_size_ * board_size_ : y * board_size_ + x;
            game_->Step(action);
        }
    }
    
    int get_current_player() {
        Player p = game_->GetCurrentPlayer();
        return p == Player::Black ? 1 : (p == Player::White ? 2 : 0); 
    }

    val get_board() {
        std::vector<int> board = game_->GetBoard();
        // 将 C++ vector 转为 JS Float64Array (或者 Int32Array)
        // 使用 typed_memory_view 需要注意生命周期，所以我们要么复制到一个 JS Array，要么使用 Float64Array。
        // 为简单起见，我们转成一个普通的 JS array
        val js_arr = val::array();
        for (size_t i = 0; i < board.size(); ++i) {
            js_arr.call<void>("push", board[i]);
        }
        return js_arr;
    }
};

EMSCRIPTEN_BINDINGS(game_module) {
    class_<WasmGameWrapper>("WasmGameWrapper")
        .constructor<std::string, int>()
        .function("play_move", &WasmGameWrapper::play_move)
        .function("sync_state", &WasmGameWrapper::sync_state)
        .function("get_current_player", &WasmGameWrapper::get_current_player)
        .function("get_board", &WasmGameWrapper::get_board);
}
