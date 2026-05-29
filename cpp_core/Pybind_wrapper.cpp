#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include "GameInterface.h"
#include "games/GoGame.h"
#include "MCTS.h"

namespace py = pybind11;

PYBIND11_MODULE(core_engine, m) {
    m.doc() = "AlphaGo Zero C++ Core Engine";

    py::enum_<Player>(m, "Player")
        .value("Black", Player::Black)
        .value("White", Player::White)
        .value("NonePlayer", Player::NonePlayer)
        .export_values();

    py::class_<GameInterface>(m, "GameInterface")
        .def("GetBoardSize", &GameInterface::GetBoardSize)
        .def("GetActionSize", &GameInterface::GetActionSize)
        .def("Reset", &GameInterface::Reset)
        .def("GetLegalMoves", &GameInterface::GetLegalMoves)
        .def("Step", &GameInterface::Step)
        .def("GetGameEnded", &GameInterface::GetGameEnded)
        .def("GetStateFeatures", &GameInterface::GetStateFeatures)
        .def("GetCurrentPlayer", &GameInterface::GetCurrentPlayer)
        .def("ToString", &GameInterface::ToString);

    py::class_<GoGame, GameInterface>(m, "GoGame")
        .def(py::init<int>(), py::arg("board_size"));

    py::class_<MCTS>(m, "MCTS")
        .def(py::init<EvalCallback, int, float>(), 
             py::arg("eval_fn"), py::arg("num_simulations"), py::arg("c_puct"))
        .def("GetActionProb", &MCTS::GetActionProb, 
             py::arg("game"), py::arg("temp") = 1.0f)
        .def("UpdateWithMove", &MCTS::UpdateWithMove, 
             py::arg("last_action"));
}
