#include "endstone_worldgen/chunk_buffer.h"
#include <algorithm>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
namespace endstone_worldgen {
namespace {
std::size_t blockCount(int min_y, int max_y) {
    if (max_y < min_y) throw std::invalid_argument("invalid y range");
    const auto height = static_cast<std::uint64_t>(static_cast<std::int64_t>(max_y) - min_y) + 1;
    if (height > std::numeric_limits<std::size_t>::max() / 256) {
        throw std::length_error("chunk height is too large");
    }
    return static_cast<std::size_t>(height) * 256;
}
} // namespace
ChunkBuffer::ChunkBuffer(ChunkPos p,int min_y,int max_y,std::uint32_t fill):pos_(p),min_y_(min_y),max_y_(max_y),blocks_(blockCount(min_y,max_y),fill){}
size_t ChunkBuffer::index(int x,int y,int z)const{if(x<0||x>15||z<0||z>15||y<min_y_||y>max_y_)throw std::out_of_range("chunk coordinate");return size_t(y-min_y_)*256+size_t(z)*16+size_t(x);}
std::uint32_t ChunkBuffer::getRuntimeId(int x,int y,int z)const{return blocks_[index(x,y,z)];}
void ChunkBuffer::setRuntimeId(int x,int y,int z,std::uint32_t id){blocks_[index(x,y,z)]=id;}
void ChunkBuffer::fill(int ax,int ay,int az,int bx,int by,int bz,std::uint32_t id){const auto low_y=std::max(min_y_,std::min(ay,by));const auto high_y=std::min(max_y_,std::max(ay,by));for(int x=std::max(0,std::min(ax,bx));x<=std::min(15,std::max(ax,bx));++x)for(std::int64_t y=low_y;y<=high_y;++y)for(int z=std::max(0,std::min(az,bz));z<=std::min(15,std::max(az,bz));++z)setRuntimeId(x,static_cast<int>(y),z,id);}
void ChunkBuffer::replace(std::uint32_t from,std::uint32_t to){std::replace(blocks_.begin(),blocks_.end(),from,to);}
void ChunkBuffer::setBiome(int x,int y,int z,std::uint32_t b){(void)index(x,y,z);biomes_[std::uint32_t((y-min_y_)/4*16+(z/4)*4+x/4)]=b;}
void ChunkBuffer::setPaletteEntry(std::uint32_t id, BlockDescriptor descriptor){palette_[id]=std::move(descriptor);}
const BlockDescriptor *ChunkBuffer::paletteEntry(std::uint32_t id)const noexcept{auto it=palette_.find(id);return it==palette_.end()?nullptr:&it->second;}
bool ChunkBuffer::hasCompletePalette()const noexcept{return std::all_of(blocks_.begin(),blocks_.end(),[this](const auto id){return palette_.contains(id);});}
std::uint64_t ChunkBuffer::fingerprint()const{
    std::uint64_t hash=14695981039346656037ULL;
    const auto byte=[&](std::uint8_t value){hash^=value;hash*=1099511628211ULL;};
    const auto u32=[&](std::uint32_t value){for(unsigned shift=0;shift<32;shift+=8)byte(static_cast<std::uint8_t>(value>>shift));};
    const auto u64=[&](std::uint64_t value){for(unsigned shift=0;shift<64;shift+=8)byte(static_cast<std::uint8_t>(value>>shift));};
    const auto text=[&](std::string_view value){u64(value.size());for(unsigned char c:value)byte(c);};

    u32(static_cast<std::uint32_t>(pos_.x));
    u32(static_cast<std::uint32_t>(pos_.z));
    u32(static_cast<std::uint32_t>(min_y_));
    u32(static_cast<std::uint32_t>(max_y_));
    u64(blocks_.size());
    for(const auto value:blocks_)u32(value);

    std::vector<std::pair<std::uint32_t,std::uint32_t>> biomes(biomes_.begin(),biomes_.end());
    std::sort(biomes.begin(),biomes.end());
    u64(biomes.size());
    for(const auto &[key,value]:biomes){u32(key);u32(value);}

    std::vector<std::pair<std::uint32_t,const BlockDescriptor *>> palette;
    palette.reserve(palette_.size());
    for(const auto &[runtime_id,descriptor]:palette_)palette.emplace_back(runtime_id,&descriptor);
    std::sort(palette.begin(),palette.end(),[](const auto &left,const auto &right){return left.first<right.first;});
    u64(palette.size());
    for(const auto &[runtime_id,descriptor]:palette){
        u32(runtime_id);
        text(descriptor->type);
        std::vector<std::pair<std::string_view,const DescriptorStateValue *>> states;
        states.reserve(descriptor->states.size());
        for(const auto &[name,value]:descriptor->states)states.emplace_back(name,&value);
        const auto utf8_less=[](std::string_view left,std::string_view right){
            return std::lexicographical_compare(left.begin(),left.end(),right.begin(),right.end(),
                [](char lhs,char rhs){return static_cast<unsigned char>(lhs)<static_cast<unsigned char>(rhs);});
        };
        std::sort(states.begin(),states.end(),[&](const auto &left,const auto &right){return utf8_less(left.first,right.first);});
        u64(states.size());
        for(const auto &[name,value]:states){
            text(name);
            byte(static_cast<std::uint8_t>(value->index()));
            std::visit([&](const auto &state){
                using State=std::decay_t<decltype(state)>;
                if constexpr(std::is_same_v<State,bool>)byte(state?1:0);
                else if constexpr(std::is_same_v<State,std::int32_t>)u32(static_cast<std::uint32_t>(state));
                else text(state);
            },*value);
        }
    }
    return hash;
}
}
