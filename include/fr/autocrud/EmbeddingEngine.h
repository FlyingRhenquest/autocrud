/**
 * Copyright 2026 Bruce Ide
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cassert>
#include <stdexcept>
#include <string>
#include <llama.h>
#include <vector>

namespace fr::autocrud {

  /**
   * Uses llama.cpp to generate embedding vectors for pgvector.
   */
  class EmbeddingEngine {

  public:

    EmbeddingEngine(const std::string& modelPath) {
      llama_backend_init();
      llama_model_params modelParams = llama_model_default_params();
      _model = llama_model_load_from_file(modelPath.c_str(), modelParams);
      if (!_model) {
        std::string err{"Failed to load "};
        err.append(modelPath);
        throw std::runtime_error(err);
      }
      llama_context_params ctxParams = llama_context_default_params();
      ctxParams.embeddings = true;
      ctxParams.n_ctx = 0; // Use model-recommended number of tokens
      _ctx = llama_init_from_model(_model, ctxParams);
      if (!_ctx) {
        throw std::runtime_error("Failed to create context");
      }
    }

    ~EmbeddingEngine() {
      if (_ctx) {
        llama_free(_ctx);
      }
      if (_model) {
        llama_model_free(_model);
      }
      llama_backend_free();
    }

    std::vector<float> getEmbeddings(const std::string &text) {
      assert(nullptr != _model);
      assert(nullptr != _ctx);
      std::vector<llama_token> tokens(text.size() + 2);
      const llama_vocab * vocab = llama_model_get_vocab(_model);
      int tokenCount = llama_tokenize(vocab, text.c_str(), text.size(), tokens.data(), tokens.size(), true, true);
      tokens.resize(tokenCount);
      llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
      if (0 != llama_decode(_ctx, batch)) {
        throw std::runtime_error("Failed to evaluate tokens");              
      }
      int embedCount = llama_model_n_embd(_model);
      float* embeddings = llama_get_embeddings(_ctx);
      std::vector<float> ret(embeddings, embeddings + embedCount);
      return ret;
    }
    
  private:
    llama_model* _model{nullptr};
    llama_context* _ctx{nullptr};
    
  };
  
}
