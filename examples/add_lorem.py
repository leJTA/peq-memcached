from pymemcache.client import base
import zstandard as zstd

def main():
   client = base.Client(('127.0.0.1', 11211))

   client.set("lorem", "Proin eleifend felis eget nisl vestibulum faucibus. Donec nibh arcu, bibendum id turpis sed, rhoncus efficitur tellus. Vivamus varius mattis risus, eu laoreet enim vehicula et. Mauris molestie porta urna, vitae maximus tellus feugiat et. Nam varius lacus non sodales tempor. Pellentesque eu elit ac massa ullamcorper venenatis. Morbi commodo, leo eget porta vestibulum, magna sapien vulputate leo, ac cursus enim magna vitae lorem. Duis mattis laoreet est, sed imperdiet tellus commodo in. Sed in massa lacus. Duis a libero ac nulla euismod blandit. Cras venenatis ultricies nibh, aliquam tincidunt ligula blandit ac. Etiam venenatis justo nisl, ac cursus leo placerat in. Nam aliquam et risus sit amet porta. In dolor nisi, vestibulum vel ultricies rhoncus, maximus vel justo. Curabitur consectetur lorem nec odio finibus volutpat. Nunc mollis eu nunc et viverra. Duis non libero tincidunt, venenatis eros ac, pellentesque eros. Sed a efficitur mauris. Suspendisse convallis, enim varius hendrerit pretium, arcu augue blandit nibh, vitae ultricies ligula dolor at dui. Nam lobortis orci in lorem elementum varius. Fusce porttitor maximus lectus, et varius tortor mollis id. Quisque hendrerit dui massa. Curabitur auctor ipsum vel eros faucibus, non elementum orci iaculis. Aliquam quis ante non massa lobortis varius.Mauris ullamcorper varius dolor ultrices maximus. Nunc in sapien nec mi scelerisque fringilla. Vestibulum eget dolor est. Sed lorem tellus, efficitur a orci dignissim, congue tincidunt elit. Aliquam ultrices tortor sit amet tincidunt malesuada. Pellentesque feugiat metus ac eros iaculis, id sagittis ex ultrices. In et urna ut erat fringilla ultricies. Fusce eget leo non libero aliquam molestie non nec magna. Donec accumsan turpis eros, pellentesque ultricies lacus hendrerit quis. Etiam quis tortor a turpis egestas efficitur. Donec magna neque, iaculis eu lacus ac, ultrices volutpat justo. Curabitur molestie tellus at erat pharetra mattis. Aliquam pulvinar, est a rutrum pellentesque, lacus tellus condimentum lorem, quis in.")

   value = client.get("lorem")

   print(f"Retrieved value of 'lorem' : {value.decode() if value else None}")

if __name__ == "__main__":
    main()

