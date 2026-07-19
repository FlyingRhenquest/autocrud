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

#include <cstddef>
#include <fr/autocrud/EmbeddingEngine.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <pqxx/pqxx>

namespace fr::autocrud {

  /**
   * A vector type to store pgvector embeddings. This is intended to
   * be one column in a table. This needs to:
   *
   * 1. Hold a float vector created by an embedding engine. The embedding
   *    engine uses a LLM model to create the vector.
   * 2. Convert the vector to a string to pass to Postgres
   * 3. On query, hold the similarity to the matched text
   *
   * @tparam parameters - Number of dimensions used by your LLM model.
   *         This will be used when creating the vector embeddings when
   *         the table is created.
   */

  template <size_t parameters>
  class Vector {
  public:
    
    Vector() {}
    ~Vector() {}

    /**
     * createExtension accepts a database connection and creates
     * the pgvector extension if it doesn't exist. Returns true
     * if the extension was created or already exists, returns
     * false if it was unable to create the vector extension.
     *
     * You DO need to be the database superuser to run this
     * command, so don't stick it in some random program that
     * any schlub can run (Like IntegrationTests...)
     */

    static bool createExtension(pqxx::connection &c) {
      bool ret = false;
      try {
        pqxx::work work(c);
        work.exec("CREATE EXTENSION IF NOT EXISTS vector;");
        work.commit();
        ret = true;
      } catch (const std::exception &e) {
        std::cerr << "Error creating database vector extension: " << std::endl;
        std::cerr << e.what() << std::endl;
      }
      return ret;
    }

    bool hasEmbeddings() {
      return _hasEmbeddings;
    }

    bool hasSimilarity() {
      return _hasSimilarity;
    }

    /**
     * Convert embeddings to a string for Postgres
     */
    std::string toPgString() const {
      bool firstElement{true};
      std::string result{"["};
      for(const auto& embedding : _embeddings) {
        if (firstElement) {
          firstElement = false;
        } else {
          result.append(",");
        }
        result.append(std::to_string(embedding));
      }
      result.append("]");
      return result;
    }

    float similarity() {
      return _similarity;
    }

    /**
     * setEmbeddings swaps with passed-in vector
     */
    
    void setEmbeddings(std::vector<float>& embeddings) {
      _hasEmbeddings = true;
      _embeddings.swap(embeddings);
    }

    /**
     * setEmbeddings with a string queries an embedding engine
     * to retrieve the vector of floats.
     */

    void setEmbeddings(const std::string &text) {
      if (!_engine) {
        throw std::runtime_error("Attempted to query Vector for a text embedding without setting up an engine beforehand.");
      }
      auto embeddings = _engine->getEmbeddings(text);
      this->setEmbeddings(embeddings);
    }

    /**
     * Used when reading the field out of the database without a special query
     */
    void parseEmbeddings(const std::string& text) {
      std::string removeBrackets = text.substr(1, text.size() - 2);
      _embeddings.clear();
      std::stringstream stream{removeBrackets};
      std::string value;
      while(std::getline(stream, value, ',')) {
        _embeddings.push_back(std::stof(value));
      }
    }

    void setSimilarity(float similarity) {
      _hasSimilarity = true;
      _similarity = similarity;
    }

    /**
     * Returns the size of the embeddings vector
     */
    size_t size() {
      return _embeddings.size();
    }

    /**
     * Set embedding engine -- this must be set prior to trying to
     * using the setEmbeddings std::string overload, as that
     * method queries the engine for the embedding.
     */
    void setEngine(std::shared_ptr<EmbeddingEngine> engine) {
      _engine = engine;
    }
 
  private:
    /**
     * True if this object is holding a vector of floats
     */
    bool _hasEmbeddings{false};
    /**
     * True if this object holds a similarity value;
     */
    bool _hasSimilarity{false};
    /**
     * The embeddings vector to send to the database
     */
    std::vector<float> _embeddings;
    /**
     * The similarity value extracted from the database
     */
    float _similarity{0.0};
    /**
     * Shared pointer to an embedding engine. This actually generates
     * the embedding vectors from a specific model. The model vector
     * dimensions need to match the parameters size passed to the
     * Vector object, but I'm not enforcing this currently.
     */
    std::shared_ptr<EmbeddingEngine> _engine;
  };
  
}
