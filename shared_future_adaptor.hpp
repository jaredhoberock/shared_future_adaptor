// Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <memory>
#include <variant>
#include <future>
#include <type_traits>

template<class Future>
class shared_future_adaptor
{
  public:
    static_assert(!std::is_copy_constructible<Future>::value && std::is_move_constructible<Future>::value,
                  "shared_future_adaptor's Future template parameter must be move-only.");

    using result_type = decltype(std::declval<Future>().get());
    
    shared_future_adaptor(Future& unique_future);
    {
      if(unique_future.is_valid())
      {
        variant_ptr_ = std::make_shared<variant_type>(std::move(unique_future));
      }
    }

  private:
    struct is_ready_visitor
    {
      bool operator()(const result_type&) const
      {
        return true;
      }

      bool operator()(const Future& underlying_future) const
      {
        return underlying_future.is_ready();
      }
    };

  public:
    bool is_ready() const
    {
      return std::visit(is_ready_visitor(), *variant_ptr_);
    }

    // if the underlying future exists, that means it is still valid and we need to use its .then()
    // the continuation would call the shared future's .get() and then recurse back to .then()
    // if the underlying future does not exist, that means that our result is ready and we should call the continuation immediately
    // the whole thing seems similar to our implementation of .get()
    //then()

  public:
    bool valid() const
    {
      return static_cast<bool>(variant_ptr_);
    }

  private:
    struct wait_visitor
    {
      void operator()(result_type&) const {}

      void operator()(Future& underlying_future) const
      {
        underlying_future.wait();
      }
    };

  public:
    void wait()
    {
      std::visit(wait_visitor(), *variant_ptr_);
    }

  private:
    struct get_visitor
    {
      variant_type& self;

      result_type& operator()(result_type& result) const
      {
        return result;
      }

      result_type& operator()(Future& underlying_future) const
      {
        // get the result from the future and store it in a temporary variable
        result_type result = underlying_future.get();

        // move the result back into the variant
        self = std::move(result);

        // recurse to actually return the result
        return std::visit(*this, self);
      }
    };

  public:
    result_type& get() const
    {
      return std::visit(*variant_ptr_, get_visitor{*this});
    }

  private:
    using state_type = std::variant<Future,result_type>;

    std::shared_ptr<state_type> variant_ptr_;
};

