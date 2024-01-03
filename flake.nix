{
  inputs.nixpkgs.url = "nixpkgs/nixos-23.11";
  outputs = { self, nixpkgs }:
    let
      lastModifiedDate = self.lastModifiedDate or self.lastModified or "19700101";
      version = builtins.substring 0 8 lastModifiedDate;

      # System types to support.
      supportedSystems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];

      # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      # Nixpkgs instantiated for supported system types.
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; overlays = [ self.overlay ]; });
    in
    {
      overlay = final: prev: {
        glxgears = with final; stdenv.mkDerivation rec {
          pname = "glxgears";
          inherit version;
          src = ./.;
          nativeBuildInputs = [ clang xorg.libX11 libGL ];
          dontConfigure = true;
          buildPhase = ''
            $CC src/xdemos/glxgears.c -o glxgears -lGL -lX11 -lm
          '';
          installPhase = "mkdir -p $out/bin; cp -r glxgears $out/bin/.";
        };
      };

      packages = forAllSystems (system:
        {
          inherit (nixpkgsFor.${system}) glxgears;
        });

      devShells = forAllSystems
        (system:
          let
            pkgs = nixpkgsFor.${system};
          in
          {
            default = pkgs.mkShell.override
              {
                stdenv = pkgs.clang16Stdenv;
              }
              {
                packages = with pkgs; [
                  xorg.libX11
                  xorg.libXrender
                  xorg.libXext
                  libGL
                  bear # Use to generate compile_commands.json for clangd 
                  gdb # Debug utility
                  clang-tools # use for clangd and clang 
                  valgrind # Use to find Memory leak
                ];
              };
          });
    };
}
